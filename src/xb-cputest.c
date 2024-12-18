#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "cputest-algorithm.h"
#include "cputest-mat.h"
#include "multitask.h"

#ifndef TEST_DURATION
#define TEST_DURATION 10
#endif

static unsigned int test_quiet;
static unsigned int test_duration;
static unsigned int test_threads;

static size_t test_case_count;
static char **test_case_list;

static void usage(FILE *f)
{
    fprintf(f, "Usage: memtest [Options] [Cases...]\n"
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

    test_case_count = argc - optind;
    test_case_list = argv + optind;
}

static void prime_task(struct mt_data *data)
{
    if (prime_count(29000) != 3153)
    {
        fprintf(stderr, "prime_count(29000) != 3153\n");
        abort();
    }
    mt_counter_inc(data);
}

static struct mt_test_ops prime_ops = {
    .warmup = prime_task,
    .test = prime_task,
};

static void fib_task(struct mt_data *data)
{
    if (fib_value(1650000) != 17024848350435039168ull)
    {
        fprintf(stderr, "fib_value(1650000) != 17024848350435039168ull\n");
        abort();
    }
    mt_counter_inc(data);
}

static struct mt_test_ops fib_ops = {
    .warmup = fib_task,
    .test = fib_task,
};

#define XORSHIFT_SEED 8675728858075378228ull

static void xorshift_task(struct mt_data *data)
{
    if (xorshift_nstep(XORSHIFT_SEED, 700000) != 7971562545477045910ull)
    {
        fprintf(stderr, "xorshift_nstep(XORSHIFT_SEED, 700000) != 7971562545477045910ull\n");
        abort();
    }

    mt_counter_inc(data);
}

static struct mt_test_ops xorshift_ops = {
    .warmup = xorshift_task,
    .test = xorshift_task,
};

#define CIRCLE_WIDTH  250
#define CIRCLE_HEIGHT 180
#define CIRCLE_RADIUS 100
#define CIRCLE_CENTER_X (CIRCLE_WIDTH / 2)
#define CIRCLE_CENTER_Y (CIRCLE_HEIGHT / 2)

static void circle_prepare(struct mt_data *data)
{
    data->userdata[0] = (uintptr_t)mt_alloc(CIRCLE_WIDTH * CIRCLE_HEIGHT * sizeof(uint32_t));
}

static void circle_clean(struct mt_data *data)
{
    mt_free((void *)data->userdata[0], CIRCLE_WIDTH * CIRCLE_HEIGHT * sizeof(uint32_t));
}

static void circle_task(struct mt_data *data)
{
    uint32_t *buffer = (uint32_t *)data->userdata[0];
    render_circle(buffer, CIRCLE_WIDTH, CIRCLE_HEIGHT, CIRCLE_CENTER_X, CIRCLE_CENTER_Y, CIRCLE_RADIUS);
    mt_counter_inc(data);
}

static struct mt_test_ops circle_ops = {
    .prepare = circle_prepare,
    .clean = circle_clean,
    .warmup = circle_task,
    .test = circle_task,
};

#define SORTI32_ELEMENTS (14 * 1024)

static int32_t sorti32_generate(uint64_t *state)
{
    *state = xorshift_next(*state);
    return (int32_t)(*state & (-1));
}

static int sorti32_compare(const void *a, const void *b)
{
    return *(int32_t *)a - *(int32_t *)b;
}

static void sorti32_prepare(struct mt_data *data)
{
    size_t element = SORTI32_ELEMENTS;
    size_t element_x2 = element * 2;
    data->userdata[0] = (uintptr_t)mt_alloc(element_x2 * sizeof(int32_t)); // unsorted data
    data->userdata[1] = data->userdata[0] + element * sizeof(int32_t);     // sort buffer
    data->userdata[2] = element;                                           // element count

    int32_t *unsorted = (int32_t *)data->userdata[0];
    uint64_t state = XORSHIFT_SEED;
    for (size_t i = 0; i < element; i++)
    {
        unsorted[i] = sorti32_generate(&state);
    }
}

static void sorti32_clean(struct mt_data *data)
{
    mt_free((void *)data->userdata[0], SORTI32_ELEMENTS * 2 * sizeof(int32_t));
}

static void sorti32_task(struct mt_data *data)
{
    int32_t *unsorted = (int32_t *)data->userdata[0];
    int32_t *buffer = (int32_t *)data->userdata[1];
    size_t element = data->userdata[2];
    memcpy(buffer, unsorted, element * sizeof(int32_t));
    qsort(buffer, element, sizeof(int32_t), sorti32_compare);
    mt_counter_inc(data);
}

static struct mt_test_ops sort_i32_ops = {
    .prepare = sorti32_prepare,
    .clean = sorti32_clean,
    .warmup = sorti32_task,
    .test = sorti32_task,
};

#define SORTU64_ELEMENTS (14 * 1024)
static uint64_t sortu64_generate(uint64_t *state)
{
    *state = xorshift_next(*state);
    return *state;
}

static int sortu64_compare(const void *a, const void *b)
{
    return *(uint64_t *)a - *(uint64_t *)b;
}

static void sortu64_prepare(struct mt_data *data)
{
    size_t element = SORTU64_ELEMENTS;
    size_t element_x2 = element * 2;
    data->userdata[0] = (uintptr_t)mt_alloc(element_x2 * sizeof(uint64_t)); // unsorted data
    data->userdata[1] = data->userdata[0] + element * sizeof(uint64_t);     // sort buffer
    data->userdata[2] = element;                                           // element count

    uint64_t *unsorted = (uint64_t *)data->userdata[0];
    uint64_t state = XORSHIFT_SEED;
    for (size_t i = 0; i < element; i++)
    {
        unsorted[i] = sortu64_generate(&state);
    }
}

static void sortu64_clean(struct mt_data *data)
{
    mt_free((void *)data->userdata[0], SORTU64_ELEMENTS * 2 * sizeof(uint64_t));
}

static void sortu64_task(struct mt_data *data)
{
    uint64_t *unsorted = (uint64_t *)data->userdata[0];
    uint64_t *buffer = (uint64_t *)data->userdata[1];
    size_t element = data->userdata[2];
    memcpy(buffer, unsorted, element * sizeof(uint64_t));
    qsort(buffer, element, sizeof(uint64_t), sortu64_compare);
    mt_counter_inc(data);
}

static struct mt_test_ops sort_u64_ops = {
    .prepare = sortu64_prepare,
    .clean = sortu64_clean,
    .warmup = sortu64_task,
    .test = sortu64_task,
};

// static const uintptr_t fpmat_add_data[] = {
//     (uintptr_t)mat_add,
//     2000,
//     2000,
//     2000,
//     2000,
// };

static const uintptr_t fpmat_mul_data[] = {
    (uintptr_t)mat_mul,
    125,
    80,
    80,
    125,
};

static const uintptr_t fpmat_conv_data[] = {
    (uintptr_t)mat_conv,
    90,
    60,
    11,
    13,
};

static void fpmat_prepare(struct mt_data *data)
{
    struct mt_shared *shared = data->shared;
    unsigned int a_row = shared->userdata[1];
    unsigned int a_col = shared->userdata[2];
    unsigned int b_row = shared->userdata[3];
    unsigned int b_col = shared->userdata[4];

    Mat *a = mat_new();
    Mat *b = mat_new();
    Mat *c = mat_new();

    mat_set_shape(a, a_row, a_col);
    mat_set_shape(b, b_row, b_col);

    data->userdata[0] = (uintptr_t)a;
    data->userdata[1] = (uintptr_t)b;
    data->userdata[2] = (uintptr_t)c;

    uint64_t state = XORSHIFT_SEED;
    float *p;


    p = a->data;
    for (unsigned int i = 0; i < a_row * a_col; i++)
    {
        *p++ = (float)xorshift_next(state) / UINT64_MAX;
    }

    p = b->data;
    for (unsigned int i = 0; i < b_row * b_col; i++)
    {
        *p++ = (float)xorshift_next(state) / UINT64_MAX;
    }
}

static void fpmat_clean(struct mt_data *data)
{
    mat_delete((Mat *)data->userdata[0]);
    mat_delete((Mat *)data->userdata[1]);
    mat_delete((Mat *)data->userdata[2]);
}

static void fpmat_task(struct mt_data *data)
{
    Mat *a = (Mat *)data->userdata[0];
    Mat *b = (Mat *)data->userdata[1];
    Mat *c = (Mat *)data->userdata[2];
    void (*mat_op)(const Mat *, const Mat *, Mat *) = (void (*)(const Mat *, const Mat *, Mat *))data->shared->userdata[0];
    mat_op(a, b, c);
    mt_counter_inc(data);
}

static struct mt_test_ops fpmat_ops = {
    .prepare = fpmat_prepare,
    .clean = fpmat_clean,
    .warmup = fpmat_task,
    .test = fpmat_task,
};

struct test_function
{
    const char *name;
    struct mt_test_ops *ops;
    const uintptr_t *userdata;
    size_t userdata_count;
};

static struct test_function test_functions[] = {
    {
        .name = "PRIME",
        .ops = &prime_ops,
    },
    {
        .name = "FIB",
        .ops = &fib_ops,
    },
    {
        .name = "XORSHIFT",
        .ops = &xorshift_ops,
    },
    {
        .name = "SORT-I32",
        .ops = &sort_i32_ops,
    },
    {
        .name = "SORT-U64",
        .ops = &sort_u64_ops,
    },
    {
        .name = "CIRCLE",
        .ops = &circle_ops,
    },
    // 这个测试意义不大，计算太简单，测试的其实主要是内存I/O
    // {
    //     .name = "FPMAT-ADD",
    //     .ops = &fpmat_ops,
    //     .userdata = fpmat_add_data,
    //     .userdata_count = sizeof(fpmat_add_data) / sizeof(fpmat_add_data[0]),
    // },
    {
        .name = "FPMAT-MUL",
        .ops = &fpmat_ops,
        .userdata = fpmat_mul_data,
        .userdata_count = sizeof(fpmat_mul_data) / sizeof(fpmat_mul_data[0]),
    },
    {
        .name = "FPMAT-CONV",
        .ops = &fpmat_ops,
        .userdata = fpmat_conv_data,
        .userdata_count = sizeof(fpmat_conv_data) / sizeof(fpmat_conv_data[0]),
    },
};

int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    if (!test_quiet)
    {
        printf("TEST                Rate\n");
    }
    if (test_case_count > 0)
    {
        for (size_t i = 0; i < test_case_count; i++)
        {
            size_t j;
            for (j = 0; j < sizeof(test_functions) / sizeof(test_functions[0]); j++)
            {
                if (strcasecmp(test_case_list[i], test_functions[j].name) == 0)
                {
                    struct mt_test_ops *ops = test_functions[j].ops;
                    const uintptr_t *userdata = test_functions[j].userdata;
                    size_t userdata_count = test_functions[j].userdata_count;

                    double r = mt_run_all_simple(ops, test_threads, test_duration, userdata, userdata_count);
                    printf("%-20s%.2f\n", test_functions[j].name, r);
                    break;
                }
            }
            if (j == sizeof(test_functions) / sizeof(test_functions[0]))
            {
                fprintf(stderr, "Unknown test case: %s\n", test_case_list[i]);
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        for (size_t j = 0; j < sizeof(test_functions) / sizeof(test_functions[0]); j++)
        {
            struct mt_test_ops *ops = test_functions[j].ops;
            const uintptr_t *userdata = test_functions[j].userdata;
            size_t userdata_count = test_functions[j].userdata_count;

            double r = mt_run_all_simple(ops, test_threads, test_duration, userdata, userdata_count);
            printf("%-20s%.2f\n", test_functions[j].name, r);
        }
    }

    return 0;
}
