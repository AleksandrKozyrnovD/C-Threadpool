#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

// #include <bits/pthreadtypes.h>
#include <stdlib.h>
#include <pthread.h>


struct threadpool_task
{
    void *(*func)(void*);
    void *res;
    void *arg;
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    int completed;
};

struct threadpool
{
    pthread_mutex_t lock;     
    pthread_cond_t notify;    
    pthread_t *threads;       
    struct threadpool_task *queue; 
    size_t thread_count;
    size_t queue_size;  
    size_t head;        
    size_t tail;        
    size_t count;       
    int shutdown; //flags
};

struct threadpool *threadpool_create(size_t thread_count, size_t queue_size);

int threadpool_destroy(struct threadpool *pool);

struct threadpool_task *threadpool_add(struct threadpool *pool, void *(*func)(void*), void *arg);

void *task_wait(struct threadpool_task *task);

#endif
