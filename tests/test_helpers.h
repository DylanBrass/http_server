//
// Created by dylanbrass on 2026-10-05.
//

#ifndef HTTP_SERVER_TEST_HELPERS_H
#define HTTP_SERVER_TEST_HELPERS_H

#include <stdlib.h>
#include <string.h>

static inline char* make_request(const char* prefix, size_t filler_len, char filler_char, const char* suffix)
{
    const size_t total = strlen(prefix) + filler_len + strlen(suffix) + 1;
    char* buf = malloc(total);
    char* p = buf;
    memcpy(p, prefix, strlen(prefix));
    p += strlen(prefix);
    memset(p, filler_char, filler_len);
    p += filler_len;
    memcpy(p, suffix, strlen(suffix) + 1);
    return buf;
}

#endif //HTTP_SERVER_TEST_HELPERS_H