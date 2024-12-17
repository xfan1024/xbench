#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>

#ifndef TEST_DURATION
#define TEST_DURATION 10
#endif

static unsigned int test_quiet;
static unsigned int test_duration;
static unsigned int test_threads;

static void usage(FILE *f)
{
    fprintf(f, "Usage: memtest [Options]\n"
                "Options:\n"
                "  -h            print this help\n"
                "  -q            print less information\n"
                "  -t <duration> Specify the duration to test\n"
                "  -T <threads>  Specify the number of threads to test\n");
}

static void parse_args(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hqt:T:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage(stdout);
            exit(EXIT_SUCCESS);
        case 'q':
            test_quiet = 1;
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

    if (test_threads == 0)
    {
        test_threads = 1;
    }
    if (test_duration == 0)
    {
        test_duration = TEST_DURATION;
    }
}


static bool is_prime(uint32_t n)
{
    if (n < 2)
    {
        return false;
    }
    uint32_t test_upper_bound = (uint32_t)sqrt(n);
    for (uint32_t i = 2; i <= test_upper_bound; i++)
    {
        if (n % i == 0)
        {
            return false;
        }
    }
    return true;
}

static size_t prime_count(uint32_t max)
{
    size_t count = 0;
    for (uint32_t i = 1; i <= max; i++)
    {
        if (is_prime(i))
        {
            count++;
        }
    }
    return count;
}

static void prime_task()
{
    if (prime_count(100000) != 9592)
    {
        fprintf(stderr, "prime_count(100000) != 9592\n");
        abort();
    }
}

struct work_thread_data_shared
{
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
    size_t prime_task_count;                // result of worker thread
};

static void *prime_thread_entry(void *arg)
{
    struct thread_data *data = (struct thread_data *)arg;
    struct work_thread_data_shared *shared = data->shared;

    // warmup
    prime_task();
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
    while (!shared->stop_flag)
    {
        prime_task();
        data->prime_task_count++;
    }

    pthread_mutex_lock(&shared->mutex);
    shared->worker_count -= 1;
    pthread_cond_signal(&shared->cond_w2m);
    pthread_mutex_unlock(&shared->mutex);
    return NULL;
}

static double do_prime_test()
{
    struct work_thread_data_shared shared =
    {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond_m2w = PTHREAD_COND_INITIALIZER,
        .cond_w2m = PTHREAD_COND_INITIALIZER,
    };
    struct thread_data *threads_data = (struct thread_data *)malloc(sizeof(struct thread_data) * test_threads);
    size_t total_tasks = 0;
    struct timespec start_time, end_time;

    for (size_t i = 0; i < test_threads; i++)
    {
        threads_data[i].index = (unsigned int)i;
        threads_data[i].shared = &shared;
        threads_data[i].prime_task_count = 0;
        pthread_create(&threads_data[i].thread, NULL, prime_thread_entry, &threads_data[i]);
    }

    // wait worker_count becomes test_threads
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

    // wait worker_count becomes 0
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
        total_tasks += threads_data[i].prime_task_count;
    }
    free(threads_data);

    uint64_t elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
    return (double)total_tasks / (elapsed_time / 1000000000.0);
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    double tasks_per_second = do_prime_test();
    if (test_quiet)
    {
        printf("%.1f\n", tasks_per_second);
    }
    else
    {
        printf("prime_count(100000) run %.1f ops/s\n", tasks_per_second);
    }
    return 0;
}
