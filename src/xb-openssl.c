#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <openssl/evp.h>
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
                "  -T <threads>  Specify the number of threads to test\n"
                ""
            );
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

// shared.userdata[0] = EVP_MD
// shared.userdata[1] test block size
// data.userdata[0] = EVP_MD_CTX
// data.userdata[1] = test memory

static void ssl_md_parpare(struct mt_data *data)
{
    struct mt_shared *shared = data->shared;
    EVP_MD *md = (EVP_MD *)shared->userdata[0];
    size_t block_size = (size_t)shared->userdata[1];
    unsigned char *memory;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, NULL);
    data->userdata[0] = (uintptr_t)ctx;
    memory = (unsigned char *)mt_alloc(block_size);
    data->userdata[1] = (uintptr_t)memory;

    for (size_t i = 0; i < block_size; i++)
    {
        memory[i] = i & 0xff;
    }
}

static void ssl_md_clean(struct mt_data *data)
{
    struct mt_shared *shared = data->shared;
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)data->userdata[0];
    size_t block_size = (size_t)shared->userdata[1];
    void *memory = (void *)data->userdata[1];

    mt_free(memory, block_size);
    EVP_MD_CTX_free(ctx);
}

static void ssl_md_test(struct mt_data *data)
{
    struct mt_shared *shared = data->shared;
    const EVP_MD *md = (const EVP_MD *)shared->userdata[0];
    size_t block_size = (size_t)shared->userdata[1];
    EVP_MD_CTX *ctx = (EVP_MD_CTX *)data->userdata[0];
    unsigned char *block = (unsigned char *)data->userdata[1];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;

    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, block, block_size);
    EVP_DigestFinal_ex(ctx, digest, &digest_len);

    mt_counter_inc(data);
}

static double do_ssl_md_test(const char *md_name, size_t block_size)
{
    const EVP_MD *md = EVP_get_digestbyname(md_name);
    if (md == NULL)
    {
        fprintf(stderr, "EVP_get_digestbyname(%s) failed\n", md_name);
        abort();
    }

    struct mt_shared shared;
    mt_shared_init(&shared);
    shared.task_prepare = ssl_md_parpare;
    shared.task_clean = ssl_md_clean;
    shared.task_warmup = ssl_md_test;
    shared.task_test = ssl_md_test;
    shared.userdata[0] = (uintptr_t)md;
    shared.userdata[1] = (uintptr_t)block_size;

    struct mt_data *data_list = (struct mt_data *)malloc(sizeof(struct mt_data) * test_threads);
    memset(data_list, 0, sizeof(struct mt_data) * test_threads);

    for (size_t i = 0; i < test_threads; i++)
    {
        data_list[i].shared = &shared;
    }

    double r = mt_run_all(data_list, test_threads, test_duration);
    free(data_list);
    return r;
}

struct test_function
{
    const char *test_name;
    const char *alg_name;
    size_t block_size;
};

static struct test_function test_functions[] = {
    {"md5-8k",      "md5",      8192},
    {"sm3-8k",      "sm3",      8192},
    {"sha1-8k",     "sha1",     8192},
    {"sha256-8k",   "sha256",   8192},
    {"sha512-8k",   "sha512",   8192},
};


int main(int argc, char *argv[])
{
    double r;
    parse_args(argc, argv);

    if (!test_quiet)
    {
        printf("TEST                Rate(ops/s)\n");
    }

    if (test_case_count > 0)
    {
        for (size_t i = 0; i < test_case_count; i++)
        {
            size_t j;
            for (j = 0; j < sizeof(test_functions) / sizeof(test_functions[0]); j++)
            {
                if (strcasecmp(test_case_list[i], test_functions[j].test_name) == 0)
                {
                    r = do_ssl_md_test(test_functions[j].alg_name, test_functions[j].block_size);
                    printf("%-20s%.2f\n", test_functions[j].test_name, r);
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
        for (size_t i = 0; i < sizeof(test_functions) / sizeof(test_functions[0]); i++)
        {
            r = do_ssl_md_test(test_functions[i].alg_name, test_functions[i].block_size);
            printf("%-20s%.2f\n", test_functions[i].test_name, r);
        }
    }

    return 0;
}
