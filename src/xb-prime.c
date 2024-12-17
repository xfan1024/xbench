#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "multitask.h"

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


static bool prime_check(uint32_t n)
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
        if (prime_check(i))
        {
            count++;
        }
    }
    return count;
}

static void prime_task(struct mt_data *data)
{
    if (prime_count(100000) != 9592)
    {
        fprintf(stderr, "prime_count(100000) != 9592\n");
        abort();
    }
    mt_counter_inc(data);
}

static double do_prime_test()
{
    struct mt_shared shared;
    double r;

    mt_shared_init(&shared);
    shared.task_test = prime_task;
    shared.task_warmup = prime_task;

    struct mt_data *data_list = (struct mt_data *)malloc(sizeof(struct mt_data) * test_threads);
    memset(data_list, 0, sizeof(struct mt_data) * test_threads);

    for (size_t i = 0; i < test_threads; i++)
    {
        data_list[i].shared = &shared;
    }

    r = mt_run_all(data_list, test_threads, test_duration);

    mt_shared_destroy(&shared);
    free(data_list);

    return r;
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
