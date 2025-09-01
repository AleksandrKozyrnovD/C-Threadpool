#include "threadpool.h"
#include <stdio.h>

static void *example(void *arg)
{
    printf("Thread %d\n", *(int *)arg);
    return NULL;
}

int main(void)
{
    struct threadpool *pool;
    int args[4] = {1, 2, 3, 4};
    struct threadpool_task *tasks[4];
    
    pool = threadpool_create(4, 4);
    
    for (int i = 0; i < 4; i++)
    {
        tasks[i] = threadpool_add(pool, example, &args[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        task_wait(tasks[i]);
    }

    int err = threadpool_destroy(pool);
    printf("Destroy result: %d\n", err);
    return 0;
}