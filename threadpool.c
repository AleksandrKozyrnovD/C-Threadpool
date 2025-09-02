#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>


static void task_init(struct threadpool_task *task, void *(*func)(void *), void *arg)
{
    task->func = func;
    task->arg = arg;
    task->res = NULL;
    task->completed = 0;
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->ready, NULL);
}

static void task_complete(struct threadpool_task *task)
{
    pthread_mutex_lock(&(task->mutex));
    task->res = task->func(task->arg);
    task->completed = 1;

    //notify waiting threads
    pthread_cond_signal(&(task->ready));
    pthread_mutex_unlock(&(task->mutex));
}

void *task_wait(struct threadpool_task *task)
{
    void *res;
    pthread_mutex_lock(&(task->mutex));
    //wait for task to complete inside worker thread
    while (!task->completed)
    {
        pthread_cond_wait(&(task->ready), &(task->mutex));
    }
    res = task->res;
    pthread_mutex_unlock(&(task->mutex));
    return res;
}

static void *threadpool_thread(void *pool)
{
    struct threadpool *tp = (struct threadpool *)pool;
    struct threadpool_task *task;

    while (1)
    {
        pthread_mutex_lock(&(tp->lock));

        //if queue is empty, wait for task or pool shutdown
        while (tp->count == 0 && !tp->shutdown)
        {
            pthread_cond_wait(&(tp->notify), &(tp->lock));
        }

        //if pool is shutdown and queue is empty, break, killing the thread
        if (tp->shutdown && tp->count == 0)
        {
            break;
        }

        task = &tp->queue[tp->head];
        tp->head = (tp->head + 1) % tp->queue_size;
        tp->count--;

        pthread_mutex_unlock(&(tp->lock));
        task_complete(task);
    }

    pthread_mutex_unlock(&(tp->lock));
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
        //create worker threads
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
    
    if (pool->threads)
    {
        free(pool->threads);
    }
    if (pool->queue)
    {
        free(pool->queue);
    }
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    free(pool);
}

int threadpool_destroy(struct threadpool *pool)
{
    if (pool == NULL) return -1;
    
    pthread_mutex_lock(&(pool->lock));
    pool->shutdown = 1;
    pthread_cond_broadcast(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));

    //wait for threads to finish, then free pool
    for (size_t i = 0; i < pool->thread_count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    
    threadpool_free(pool);
    return 0;
}

struct threadpool_task *threadpool_add(struct threadpool *pool, void *(*func)(void *), void *arg)
{
    if (pool == NULL || func == NULL) return NULL;

    pthread_mutex_lock(&(pool->lock));
    if (pool->count == pool->queue_size || pool->shutdown)
    {
        pthread_mutex_unlock(&(pool->lock));
        return NULL;
    }

    int next_tail = (pool->tail + 1) % pool->queue_size;
    task_init(&pool->queue[pool->tail], func, arg);
    struct threadpool_task *task = &pool->queue[pool->tail];
    
    pool->tail = next_tail;
    pool->count++;
    
    //notify worker thread
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
    return task;
}
