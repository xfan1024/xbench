#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>

#ifdef HAVE_NUMA
#include <numa.h>
#endif

void *memtest_alloc(size_t size)
{
#ifdef HAVE_NUMA
    if (numa_available() >= 0)
    {
        return numa_alloc_local(size);
    }
#endif
    return malloc(size);
}

void memtest_free(void *ptr, size_t size)
{
#ifdef HAVE_NUMA
    if (numa_available() >= 0)
    {
        numa_free(ptr, size);
        return;
    }
#endif
    (void)size;
    free(ptr);
}

#ifndef MAGIC_NOT_ZERO
#define MAGIC_NOT_ZERO 1
#endif

#if MAGIC_NOT_ZERO
#define MAGIC 8675728858075378228ull
#else
#define MAGIC 0ull
#endif

#ifndef TEST_GB
#define TEST_GB 8
#endif

#ifndef TEST_DURATION
#define TEST_DURATION 10
#endif

struct test_function
{
    const char *name;
    void (*func)(void *, size_t);
};

struct test_result
{
    double best_rate;
    double avg_rate;
    double worst_rate;
};

void test_copy(void *memory, size_t size)
{
    size_t count = size / sizeof(uint64_t);
    size_t half = count / 2;
    uint64_t *load_ptr = (uint64_t *)memory;
    uint64_t *store_ptr = load_ptr + half;

    for (size_t i = 0; i < half; i++)
    {
        store_ptr[i] = load_ptr[i];
    }
}

void test_load(void *memory, size_t size)
{
    size_t count = size / sizeof(uint64_t);
    uint64_t *load_ptr = (uint64_t *)memory;
    uint64_t sum = 0;
    volatile uint64_t result;
    for (size_t i = 0; i < count; i++)
    {
        sum ^= load_ptr[i];
    }
    /* prevent the compiler from optimizing out loop memory access */
    result = sum;
    (void)result;

}

void test_store(void *memory, size_t size)
{
    size_t count = size / sizeof(uint64_t);
    uint64_t *store_ptr = (uint64_t *)memory;

    for (size_t i = 0; i < count; i++)
    {
        store_ptr[i] = MAGIC;
    }
}

void test_memset(void *memory, size_t size)
{
    memset(memory, 0, size);
}

void test_memcpy(void *memory, size_t size)
{
    size_t count = size / sizeof(uint64_t);
    size_t half = count / 2;
    uint64_t *load_ptr = (uint64_t *)memory;
    uint64_t *store_ptr = load_ptr + half;

    memcpy(store_ptr, load_ptr, half * sizeof(uint64_t));
}

double to_megabytes_per_second(size_t size, uint64_t duration)
{
    return (double)size / duration * 1000000000 / 1024 / 1024;
}

void memtest_init(void *memory, size_t size)
{
    size_t count = size / sizeof(uint64_t);
    uint64_t *ptr = (uint64_t *)memory;
    for (size_t i = 0; i < count; i++)
    {
        ptr[i] = MAGIC;
    }
}

unsigned int test_quiet;
unsigned int test_gb;
unsigned int test_duration;
unsigned int test_threads;

size_t test_case_count;
char **test_case_list;

void usage(FILE *f)
{
    fprintf(f, "Usage: memtest [Options] [Cases...]\n"
                "Options:\n"
                "  -h            print this help\n"
                "  -q            print less information\n"
                "  -g <gb>       Specify the size of memory to test in GB\n"
                "  -t <duration> Specify the duration to test\n"
                "  -T <threads>  Specify the number of threads to test\n"
                "Cases:\n"
                "  COPY          for loop copy memory from some where to another\n"
                "  STORE         for loop store some value to memory\n"
                "  LOAD          for loop load some value from memory\n"
                "  MEMSET        test libc memset performance\n"
                "  MEMCPY        test libc memcpy performance\n");
}

void parse_args(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hqG:t:T:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage(stdout);
            exit(EXIT_SUCCESS);
        case 'q':
            test_quiet = 1;
            break;
        case 'G':
            test_gb = atoi(optarg);
            break;
        case 't':
            test_duration = atoi(optarg);
            break;
        case 'T':
            test_threads = atoi(optarg);
            break;
        default:
            usage(stderr);
            exit(EXIT_FAILURE);
        }
    }
    test_case_count = argc - optind;
    test_case_list = argv + optind;
    if (test_gb == 0)
    {
        test_gb = TEST_GB;
    }
    if (test_threads == 0)
    {
        test_threads = 1;
    }
    if (test_duration == 0)
    {
        test_duration = TEST_DURATION;
    }
}

struct work_thread_data_shared
{
    void (*memtest_func)(void *, size_t);
    volatile unsigned int stop_flag;        // when set, worker thread should stop
    pthread_mutex_t mutex;                  // protect worker_count and cond_m2w, cond_w2m
    unsigned int worker_count;
    unsigned int start_fence;
    pthread_cond_t cond_m2w;                // main thread to worker thread
    pthread_cond_t cond_w2m;                // worker thread to main thread
};

struct thread_data
{
    unsigned int index;
    pthread_t thread;
    struct work_thread_data_shared *shared;
    size_t mem_size;                        // usually 2GB
    size_t worker_access_bytes;             // result of worker thread
};

void *memtest_thread_entry(void *arg)
{
    struct thread_data *data = (struct thread_data *)arg;
    struct work_thread_data_shared *shared = data->shared;
    void *mem_data = memtest_alloc(data->mem_size);

    // warmup
    memtest_init(mem_data, data->mem_size);
    shared->memtest_func(mem_data, data->mem_size);
    pthread_mutex_lock(&shared->mutex);
    shared->worker_count += 1;
    pthread_cond_signal(&shared->cond_w2m);
    pthread_mutex_unlock(&shared->mutex);

    // wait for start
    pthread_mutex_lock(&shared->mutex);
    while (!shared->start_fence)
    {
        pthread_cond_wait(&shared->cond_m2w, &shared->mutex);
    }
    pthread_mutex_unlock(&shared->mutex);

    // run test until stop_flag is set
    while (1)
    {
        // check stop_flag every 32MB
        uint8_t *data_p = (uint8_t *)mem_data;
        size_t remaining = data->mem_size;
        const size_t max_size = 32 * 1024 * 1024;

        while (remaining)
        {
            size_t size = remaining > max_size ? max_size : remaining;
            shared->memtest_func(data_p, size);
            data_p += size;
            remaining -= size;
            data->worker_access_bytes += size;
            if (shared->stop_flag)
            {
                goto out;
            }
        }

    }
out:

    pthread_mutex_lock(&shared->mutex);
    shared->worker_count -= 1;
    pthread_cond_signal(&shared->cond_w2m);
    pthread_mutex_unlock(&shared->mutex);
    memtest_free(mem_data, data->mem_size);
    return NULL;
}

double do_memory_test(size_t mem_size, void (*memtest_func)(void *, size_t))
{
    struct work_thread_data_shared shared =
    {
        .memtest_func = memtest_func,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond_m2w = PTHREAD_COND_INITIALIZER,
        .cond_w2m = PTHREAD_COND_INITIALIZER,
    };
    struct thread_data *threads_data = (struct thread_data *)malloc(sizeof(struct thread_data) * test_threads);
    size_t per_thread_size = ((mem_size / test_threads) & ~0xff);
    size_t total_bytes = 0;
    struct timespec start_time, end_time;

    for (size_t i = 0; i < test_threads; i++)
    {
        threads_data[i].index = (unsigned int)i;
        threads_data[i].shared = &shared;
        threads_data[i].mem_size = per_thread_size;
        threads_data[i].worker_access_bytes = 0;
        pthread_create(&threads_data[i].thread, NULL, memtest_thread_entry, &threads_data[i]);
    }

    // wait worker_count becames test_threads
    pthread_mutex_lock(&shared.mutex);
    while (shared.worker_count != test_threads)
    {
        pthread_cond_wait(&shared.cond_w2m, &shared.mutex);
    }
    pthread_mutex_unlock(&shared.mutex);

    // record start time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // notify all worker threads to start
    pthread_mutex_lock(&shared.mutex);
    shared.start_fence = 1;
    pthread_cond_broadcast(&shared.cond_m2w);
    pthread_mutex_unlock(&shared.mutex);


    // run test for test_duration seconds
    sleep(test_duration);

    // notify all worker threads to stop
    shared.stop_flag = 1;

    // wait worker_count becames 0
    pthread_mutex_lock(&shared.mutex);
    while (shared.worker_count != 0)
    {
        pthread_cond_wait(&shared.cond_w2m, &shared.mutex);
    }
    pthread_mutex_unlock(&shared.mutex);

    // record end time
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // release threads_data
    for (size_t i = 0; i < test_threads; i++)
    {
        pthread_join(threads_data[i].thread, NULL);
        total_bytes += threads_data[i].worker_access_bytes;
    }
    free(threads_data);

    uint64_t elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
    return to_megabytes_per_second(total_bytes, elapsed_time);
}

struct test_function test_functions[] = {
    {"COPY", test_copy},
    {"STORE", test_store},
    {"LOAD", test_load},
    {"MEMSET", test_memset},
    {"MEMCPY", test_memcpy},
};

int main(int argc, char *argv[])
{
    double r;
    size_t mem_size;
    size_t function_count = sizeof(test_functions) / sizeof(test_functions[0]);

    parse_args(argc, argv);
    mem_size = 1024ull * 1024ull * 1024ull * test_gb;

    if (!test_quiet)
    {
        printf("MAGIC_NOT_ZERO=%d test_gb=%u\n", MAGIC_NOT_ZERO, test_gb);
        printf("TEST                Rate\n");
    }
    if (test_case_count > 0)
    {
        for (size_t i = 0; i < test_case_count; i++)
        {
            size_t j;
            for (j = 0; j < function_count; j++)
            {
                if (strcasecmp(test_case_list[i], test_functions[j].name) == 0)
                {
                    r = do_memory_test(mem_size, test_functions[j].func);
                    printf("%-20s%.2f\n", test_functions[j].name, r);
                    break;
                }
            }
            if (j == function_count)
            {
                fprintf(stderr, "Unknown test case: %s\n", test_case_list[i]);
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < function_count; i++)
        {
            r = do_memory_test(mem_size, test_functions[i].func);
            printf("%-20s%.2f\n", test_functions[i].name, r);
        }
    }
}
