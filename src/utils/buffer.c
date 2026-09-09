//
// Created by dylanbrass on 2026-09-08.
//
#include "buffer.h"

#include <stdlib.h>

#include "httpserver.h"

int allocate_buffer(Buffer* buffer, const size_t new_size)
{
    if (new_size <= buffer->capacity)
    {
        return 0;
    }
    size_t new_capacity = buffer->capacity == 0 ? INIT_BUFFER_SIZE : buffer->capacity * 2;
    while (new_capacity < new_size)
    {
        new_capacity *= 2;
    }

    char* new_data = realloc(buffer->data, new_capacity);
    if (new_data == nullptr)
    {
        return -1;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return 0;
}

void free_buffer(Buffer* buffer)
{
    free(buffer->data);
    buffer->data = nullptr;
    buffer->capacity = 0;
}
