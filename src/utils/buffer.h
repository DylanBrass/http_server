//
// Created by dylanbrass on 2026-09-08.
//

#ifndef HTTP_SERVER_BUFFER_H
#define HTTP_SERVER_BUFFER_H
#include <stddef.h>

#define GROWTH_CHUNK 4096

typedef struct
{
    char* data;
    size_t capacity;
} Buffer;

int allocate_buffer(Buffer* buffer, size_t new_size);
void free_buffer(Buffer* buffer);

#endif //HTTP_SERVER_BUFFER_H