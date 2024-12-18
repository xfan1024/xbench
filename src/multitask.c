#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "multitask.h"

void mt_shared_init(struct mt_shared *shared)
{
    memset(shared, 0, sizeof(struct mt_shared));
    pthread_mutex_init(&shared->mutex, NULL);
    pthread_cond_init(&shared->cond_m2w, NULL);
    pthread_cond_init(&shared->cond_w2m, NULL);
}

void mt_shared_destroy(struct mt_shared *shared)
{
    pthread_mutex_destroy(&shared->mutex);
    pthread_cond_destroy(&shared->cond_m2w);
    pthread_cond_destroy(&shared->cond_w2m);
}

static void *mt_worker_entry(void *arg)
{
    struct mt_data *data = (struct mt_data *)arg;
    struct mt_shared *shared = data->shared;
    const struct mt_test_ops *ops = shared->ops;

    // prepare
    if (ops->prepare)
    {
        ops->prepare(data);
    }

    // warmup
    if (ops->warmup)
    {
        ops->warmup(data);
    }

    // notify main thread that worker thread is ready
    pthread_mutex_lock(&shared->mutex);
    shared->worker_count += 1;
    if (shared->worker_count == shared->workers)
    {
        pthread_cond_signal(&shared->cond_w2m);
    }
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
        ops->test(data);
    }

    // notify main thread that worker thread is stopped
    pthread_mutex_lock(&shared->mutex);
    shared->worker_count -= 1;
    if (shared->worker_count == 0)
    {
        pthread_cond_signal(&shared->cond_w2m);
    }
    pthread_mutex_unlock(&shared->mutex);

    if (ops->clean)
    {
        ops->clean(data);
    }
    return NULL;

}

double mt_run_all(struct mt_data *data_list, unsigned int tasks, unsigned int duration)
{
    struct mt_shared *shared = NULL;
    struct timespec start_time, end_time;
    uint64_t elapsed;
    uint64_t counter = 0;

    if (!tasks)
    {
        fprintf(stderr, "tasks cannot be 0\n");
        abort();
    }

    for (unsigned int i = 0; i < tasks; i++)
    {
        struct mt_data *data = &data_list[i];
        if (i == 0)
        {
            shared = data->shared;
            if (shared->ops == NULL)
            {
                fprintf(stderr, "shared->ops is not set\n");
                abort();
            }
            if (shared->ops->test == NULL)
            {
                fprintf(stderr, "shared->ops->test is not set\n");
                abort();
            }
            shared->workers = tasks;
        }
        else if (shared != data->shared)
        {
            fprintf(stderr, "shared data is not the same\n");
            abort();
        }

        data->index = i;
        pthread_create(&data->thread, NULL, mt_worker_entry, data);
    }

    // wait worker_count becomes test_threads
    pthread_mutex_lock(&shared->mutex);
    while (shared->worker_count != tasks)
    {
        pthread_cond_wait(&shared->cond_w2m, &shared->mutex);
    }
    pthread_mutex_unlock(&shared->mutex);

    // record start time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // notify all worker threads to start
    pthread_mutex_lock(&shared->mutex);
    shared->start_fence = 1;
    pthread_cond_broadcast(&shared->cond_m2w);
    pthread_mutex_unlock(&shared->mutex);

    // run test for duration seconds
    sleep(duration);

    // notify all worker threads to stop
    shared->stop_flag = 1;

    // wait worker_count becomes 0
    pthread_mutex_lock(&shared->mutex);
    while (shared->worker_count != 0)
    {
        pthread_cond_wait(&shared->cond_w2m, &shared->mutex);
    }
    pthread_mutex_unlock(&shared->mutex);

    // record end time
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // join all worker threads
    for (unsigned int i = 0; i < tasks; i++)
    {
        counter += data_list[i].counter;
        pthread_join(data_list[i].thread, NULL);
    }

    elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
    return (double) counter / (elapsed / 1000000000.0);
}

double mt_run_all_simple(const struct mt_test_ops *ops, unsigned int tasks, unsigned int duration, const uintptr_t *userdata, uintptr_t userdata_count)
{
    struct mt_shared shared;
    double r;

    mt_shared_init(&shared);
    if (userdata && userdata_count)
    {
        if (userdata_count > sizeof(shared.userdata) / sizeof(shared.userdata[0]))
        {
            fprintf(stderr, "userdata_count is too large\n");
            abort();
        }
        memcpy(shared.userdata, userdata, sizeof(uintptr_t) * userdata_count);
    }

    shared.ops = ops;

    struct mt_data *data_list = (struct mt_data *)malloc(sizeof(struct mt_data) * tasks);
    memset(data_list, 0, sizeof(struct mt_data) * tasks);

    for (size_t i = 0; i < tasks; i++)
    {
        data_list[i].shared = &shared;
    }

    r = mt_run_all(data_list, tasks, duration);

    mt_shared_destroy(&shared);
    free(data_list);
    return r;
}


#ifdef HAVE_NUMA
#include <numa.h>
#endif

void *mt_alloc(size_t size)
{
#ifdef HAVE_NUMA
    if (numa_available() >= 0)
    {
        return numa_alloc_local(size);
    }
#endif
    return malloc(size);
}

void mt_free(void *ptr, size_t size)
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
