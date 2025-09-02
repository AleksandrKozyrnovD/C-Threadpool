# C-Threadpool

This implementation provides a thread pool for managing concurrent task execution in C. It allows you to create a pool of worker threads that process tasks from a queue, making it efficient for handling multiple operations concurrently without creating and destroying threads for each task. Testing is available at [here](main.c).


## Features
- Fixed-size thread pool: Create a pool with a specified number of worker threads
- Bounded task queue: Tasks are stored in a circular queue with a fixed capacity
- Synchronous task execution: Wait for task completion and retrieve results
- Thread-safe operations: All operations are properly synchronized using mutexes and condition variables
- Graceful shutdown: Proper cleanup of resources when destroying the pool

## Usage example
1) Copy `threadpool.h` and `threadpool.c` in your project
2) Include `threadpool.h`
3) Compile project with `threadpool.c`

```c
#include "threadpool.h"
#include <stdio.h>

void* example_task(void* arg) {
    int* value = (int*)arg;
    printf("Processing value: %d\n", *value);
    return (void*)(*value * 2);
}

int main() {
    // Create a thread pool with 4 threads and queue size of 10
    struct threadpool* pool = threadpool_create(4, 10);
    
    int values[] = {1, 2, 3, 4, 5};
    struct threadpool_task* tasks[5];
    
    // Add tasks to the pool
    for (int i = 0; i < 5; i++) {
        tasks[i] = threadpool_add(pool, example_task, &values[i]);
    }
    
    // Wait for tasks to complete and get results
    for (int i = 0; i < 5; i++) {
        int result = (int)task_wait(tasks[i]);
        printf("Task %d result: %d\n", i, result);
    }
    
    // Clean up
    threadpool_destroy(pool);
    return 0;
}
```
