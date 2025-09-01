#include "threadpool.h"
#include <pthread.h>


static void task_init(struct threadpool_task *task, void *(*func)(void *))
{
    task->func = func;
    task->res = NULL;
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->ready, NULL);
    task->completed = 0;
}

static void task_complete(struct threadpool_task *task)
{
    pthread_mutex_lock(&(task->mutex));
    task->res = task->func(task->arg);
    task->completed = 1;
    pthread_cond_signal(&(task->ready));
    pthread_mutex_unlock(&(task->mutex));
}

void *task_wait(struct threadpool_task *task)
{
    pthread_mutex_lock(&(task->mutex));
    while (!task->completed)
    {
        pthread_cond_wait(&(task->ready), &(task->mutex));
    }
    pthread_mutex_unlock(&(task->mutex));
    return task->res;
}

static void *threadpool_thread(void *pool)
{
    struct threadpool *tp = (struct threadpool *)pool;
    struct threadpool_task task;
    while (1)
    {
        pthread_mutex_lock(&(tp->lock));
        while (tp->count == 0 && !tp->shutdown)
        {
            pthread_cond_wait(&(tp->notify), &(tp->lock));
        }
        if (tp->shutdown)
        {
            break;
        }
        task = tp->queue[tp->head];
        tp->head = (tp->head + 1) % tp->queue_size;
        tp->count--;
        pthread_mutex_unlock(&(tp->lock));
        task.func(task.arg);
    }
    pthread_mutex_unlock(&(tp->lock));
    pthread_cond_broadcast(&(tp->notify));

    pthread_exit(NULL);

    return NULL;
}

struct threadpool *threadpool_create(size_t thread_count, size_t queue_size)
{
    struct threadpool *pool;
    int tid;
    
    if (thread_count <= 0 || queue_size <= 0)
    {
        return NULL;
    }
    
    pool = (struct threadpool *)malloc(sizeof(struct threadpool));
    if (pool == NULL)
    {
        return NULL;
    }
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (pool->threads == NULL)
    {
        free(pool);
        return NULL;
    }
    pool->queue = (struct threadpool_task *)malloc(sizeof(struct threadpool_task) * queue_size);
    if (pool->queue == NULL)
    {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = 0;
    
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 ||
        pthread_cond_init(&(pool->notify), NULL) != 0 ||
        pool->threads == NULL || pool->queue == NULL)
        {
        if (pool)
        {
            free(pool->threads);
            free(pool->queue);
            free(pool);
        }
        return NULL;
    }
    for (size_t i = 0; i < thread_count; i++)
    {
        if ((tid = pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool)) != 0)
        {
            threadpool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}


static void threadpool_free(struct threadpool *pool)
{
    if (pool == NULL)
    {
        return;
    }
    
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    
    if (pool->threads)
    {
        free(pool->threads);
    }
    if (pool->queue)
    {
        free(pool->queue);
    }
    free(pool);
}


int threadpool_destroy(struct threadpool *pool)
{
    int err = 0;
    
    if (pool == NULL)
    {
        return -1;
    }
    
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return -1;
    }
    
    if (pool->shutdown)
    {
        return -1;
    }
    
    pool->shutdown = 1;
    
    if ((pthread_cond_broadcast(&(pool->notify)) != 0 ||
        (pthread_mutex_unlock(&(pool->lock)) != 0)))
    {
        err = -1;
        goto out;
    }
    
    for (size_t i = 0; i < pool->thread_count; i++)
    {
        if (pthread_join(pool->threads[i], NULL) != 0)
        {
            err = -1;
        }
    }
    
out:
    threadpool_free(pool);
    return err;
}

struct threadpool_task *threadpool_add(struct threadpool *pool, void *(*func)(void *), void *arg)
{
    int next;
    struct threadpool_task task;
    if (pool == NULL || func == NULL)
    {
        return NULL;
    }
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return NULL;
    }
    next = (pool->tail + 1) % pool->queue_size;
    if (pool->count == pool->queue_size)
    {
        goto out;
    }
    if (pool->shutdown)
    {
        goto out;
    }
    task_init(&(pool->queue[pool->tail]), func);
    pool->queue[pool->tail].arg = arg;
    pool->tail = next;
    pool->count++;
    if (pthread_cond_signal(&(pool->notify)) != 0)
    {
        goto out;
    }
out:
    if (pthread_mutex_unlock(&(pool->lock)) != 0)
    {
        return NULL;
    }
    return &(pool->queue[pool->tail - 1]);
}

