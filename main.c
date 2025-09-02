#include "threadpool.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *simple_task(void *arg)
{
    int *value = (int*)arg;
    *value *= 2;
    return NULL;
}

static void *string_task(void *arg)
{
    char *str = (char*)arg;
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - 'a' + 'A';
        }
    }
    return str;
}

static void *addition_task(void *arg)
{
    int *values = (int*)arg;
    values[2] = values[0] + values[1];
    return &values[2];
}

static void *sleep_task(void *arg)
{
    sleep(1);
    return arg;
}

int main(void)
{
    // Test1: Create and destroy threadpool
    {
        struct threadpool *pool = threadpool_create(2, 10);
        if (pool == NULL) {
            printf("Test1 Failed: Could not create threadpool\n");
        } else {
            if (threadpool_destroy(pool) != 0) {
                printf("Test1 Failed: Could not destroy threadpool\n");
            } else {
                printf("Test1 Passed: Threadpool created and destroyed successfully\n");
            }
        }
    }

    // Test2: Execute single task
    {
        struct threadpool *pool = threadpool_create(2, 10);
        int value = 42;
        struct threadpool_task *task = threadpool_add(pool, simple_task, &value);
        
        if (task == NULL) {
            printf("Test2 Failed: Could not add task\n");
            threadpool_destroy(pool);
        } else {
            void *result = task_wait(task);
            if (value == 84 && result == NULL) {
                printf("Test2 Passed: Task executed correctly\n");
            } else {
                printf("Test2 Failed: Expected 84 and NULL, got %d and %p\n", value, result);
            }
            threadpool_destroy(pool);
        }
    }

    // Test3: Execute string conversion task
    {
        struct threadpool *pool = threadpool_create(2, 10);
        char str[] = "hello";
        struct threadpool_task *task = threadpool_add(pool, string_task, str);
        
        if (task == NULL) {
            printf("Test3 Failed: Could not add task\n");
            threadpool_destroy(pool);
        } else {
            char *result = (char*)task_wait(task);
            if (strcmp(result, "HELLO") == 0) {
                printf("Test3 Passed: String converted successfully\n");
            } else {
                printf("Test3 Failed: Expected 'HELLO', got '%s'\n", result);
            }
            threadpool_destroy(pool);
        }
    }

    // Test4: Execute multiple tasks
    {
        struct threadpool *pool = threadpool_create(4, 10);
        int data[3] = {5, 7, 0}; // [0] and [1] are inputs, [2] is output
        struct threadpool_task *task = threadpool_add(pool, addition_task, data);
        
        if (task == NULL) {
            printf("Test4 Failed: Could not add task\n");
            threadpool_destroy(pool);
        } else {
            int *result = (int*)task_wait(task);
            if (data[2] == 12 && result == &data[2]) {
                printf("Test4 Passed: Addition task executed correctly\n");
            } else {
                printf("Test4 Failed: Expected 12 and %p, got %d and %p\n", 
                       &data[2], data[2], result);
            }
            threadpool_destroy(pool);
        }
    }

    // Test5: Handle full queue
    {
        struct threadpool *pool = threadpool_create(1, 2);
        int values[3] = {1, 2, 3};
        
        // Add tasks until queue is full
        struct threadpool_task *task1 = threadpool_add(pool, simple_task, &values[0]);
        struct threadpool_task *task2 = threadpool_add(pool, simple_task, &values[1]);
        struct threadpool_task *task3 = threadpool_add(pool, simple_task, &values[2]);
        
        if (task3 == NULL) {
            printf("Test5 Passed: Correctly rejected task when queue full\n");
        } else {
            printf("Test5 Failed: Should have rejected task when queue full\n");
            task_wait(task3);
        }
        
        if (task1) task_wait(task1);
        if (task2) task_wait(task2);
        threadpool_destroy(pool);
    }

    // Test6: Handle destroying pool while tasks are running
    {
        struct threadpool *pool = threadpool_create(3, 4);
        
        // Add tasks until queue is full
        struct threadpool_task *task1 = threadpool_add(pool, sleep_task, NULL);
        struct threadpool_task *task2 = threadpool_add(pool, sleep_task, NULL);
        struct threadpool_task *task3 = threadpool_add(pool, sleep_task, NULL);
        
        int err = threadpool_destroy(pool);
        if (err == 0) {
            printf("Test6 Passed: Correctly destroyed pool while tasks running\n");
        } else {
            printf("Test6 Failed: Should have destroyed pool while tasks running\n");
        }
    }

    // Test7: Handle large queue
    {
        struct threadpool *pool = threadpool_create(4, 100);
        
        for (int i = 0; i < 50; i++) {
            threadpool_add(pool, sleep_task, NULL);
        }
        
        int err = threadpool_destroy(pool);
        if (err == 0) {
            printf("Test7 Passed: Correctly handled large queue\n");
        } else {
            printf("Test7 Failed: Should have handled large queue\n");
        }
    }

    return 0;
}
