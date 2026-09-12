//
// Created by dylanbrass on 2026-09-12.
//

#ifndef HTTP_SERVER_QUEUE_H
#define HTTP_SERVER_QUEUE_H
#include <pthread.h>
#include "httpserver.h"


typedef struct
{
    int fds[MAX_CONNECTIONS];
    int next_push;
    int next_pop;
    int count;
    bool is_shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ConnectionQueue;

int queue_init(ConnectionQueue* queue);

int pop_connection(ConnectionQueue* queue);

int push_connection(ConnectionQueue* queue, int fd);

int queue_destroy(ConnectionQueue* queue);

int queue_shutdown(ConnectionQueue* queue);

#endif //HTTP_SERVER_QUEUE_H