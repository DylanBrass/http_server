//
// Created by dylanbrass on 2026-09-12.
//
#include "queue.h"

int queue_init(ConnectionQueue* queue)
{
    queue->count = 0;
    queue->next_pop = 0;
    queue->next_push = 0;
    queue->is_shutdown = false;

    if (pthread_mutex_init(&queue->mutex, nullptr) != 0)
    {
        return -1;
    }

    if (pthread_cond_init(&queue->not_empty, nullptr) != 0)
    {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    if (pthread_cond_init(&queue->not_full, nullptr) != 0)
    {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    return 0;
}

int push_connection(ConnectionQueue* queue, int fd)
{
    //lock the queue state so no other thread can modify it at the same time
    pthread_mutex_lock(&queue->mutex);

    while (queue->count >= MAX_CONNECTIONS)
    {
        // Make the thread sleep if the queue is full, wait for a signal to say not full
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    //Add fd to queue
    queue->fds[queue->next_push] = fd;
    queue->count++;
    queue->next_push = (queue->next_push + 1) % MAX_CONNECTIONS;

    // Wake up waiting threads (in this case a pop waiting to have an item)
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int pop_connection(ConnectionQueue* queue)
{
    //lock the queue state so no other thread can modify it at the same time
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0 && !queue->is_shutdown)
    {
        // Make the thread sleep if the queue is empty, wait for a signal to say not empty
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->count == 0)
    {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    const int value = queue->fds[queue->next_pop];
    queue->count--;
    queue->next_pop = (queue->next_pop + 1) % MAX_CONNECTIONS;

    // Wake up waiting threads (in this case a push waiting to have an item)
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return value;
}

int queue_destroy(ConnectionQueue* queue)
{
    bool error = false;

    if (pthread_mutex_destroy(&queue->mutex) != 0)
    {
        error = true;
    }

    if (pthread_cond_destroy(&queue->not_empty) != 0)
    {
        error = true;
    }

    if (pthread_cond_destroy(&queue->not_full) != 0)
    {
        error = true;
    }

    return error ? -1 : 0;
}

int queue_shutdown(ConnectionQueue* queue)
{
    pthread_mutex_lock(&queue->mutex);

    queue->is_shutdown = true;

    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}
