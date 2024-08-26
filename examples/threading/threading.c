#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg, ...) printf("threading: " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param)
{
    struct thread_data *t_data = (struct thread_data *)thread_param;
    t_data->thread_complete_success = false; // short circuit false

    if ((0 != usleep(t_data->wait_to_obtain_ms * 1000)) ||
        (0 != pthread_mutex_lock(t_data->mutex)) ||
        (0 != usleep(t_data->wait_to_release_ms * 1000)) ||
        (0 != pthread_mutex_unlock(t_data->mutex)))
    {
        return thread_param;
    }

    t_data->thread_complete_success = true;
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *t_data = malloc(sizeof(struct thread_data));
    if (NULL == t_data)
    {
        return false;
    }

    t_data->wait_to_obtain_ms = wait_to_obtain_ms;
    t_data->wait_to_release_ms = wait_to_release_ms;
    t_data->mutex = mutex;

    return (0 == pthread_create(thread, NULL, threadfunc, (void *)t_data));
}
