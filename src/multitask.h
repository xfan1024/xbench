#ifndef __multitask_h__
#define __multitask_h__

#include <stdint.h>
#include <pthread.h>

struct mt_data;
typedef void (*mt_func)(struct mt_data*);

struct mt_test_ops
{
    mt_func prepare;
    mt_func clean;
    mt_func warmup;
    mt_func test;
};

struct mt_shared
{
    const struct mt_test_ops *ops;
    volatile unsigned int stop_flag;        // when set, worker thread should stop
    pthread_mutex_t mutex;                  // protect worker_count and cond_m2w, cond_w2m
    pthread_cond_t cond_m2w;                // main thread to worker thread
    pthread_cond_t cond_w2m;                // worker thread to main thread
    unsigned int worker_count;
    unsigned int start_fence;
    unsigned int workers;

    // tester use:
    uintptr_t userdata[8];
};

struct mt_data
{
    unsigned int index;
    pthread_t thread;
    struct mt_shared *shared;
    uint64_t counter;

    // tester use:
    uintptr_t userdata[8];
};


void mt_shared_init(struct mt_shared *shared);
void mt_shared_destroy(struct mt_shared *shared);

// return rate in counter per second
double mt_run_all(struct mt_data *data_list, unsigned int tasks, unsigned int duration);
double mt_run_all_simple(const struct mt_test_ops *ops, unsigned int tasks, unsigned int duration, const uintptr_t *userdata, uintptr_t userdata_count);

static inline void mt_counter_inc(struct mt_data *data)
{
    data->counter++;
}

static inline void mt_counter_add(struct mt_data *data, unsigned int value)
{
    data->counter += value;
}

// support numa allocate
void *mt_alloc(size_t size);
void mt_free(void *ptr, size_t size);

#endif
