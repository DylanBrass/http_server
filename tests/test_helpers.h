//
// Created by dylanbrass on 2026-10-05.
//

#ifndef HTTP_SERVER_TEST_HELPERS_H
#define HTTP_SERVER_TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

static int test_failures = 0;

static inline void run_test(const char* name, void (*test)(void))
{
    fflush(stdout);
    const pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        printf("FAIL: %s (could not fork)\n", name);
        test_failures++;
        return;
    }

    if (pid == 0)
    {
        test();
        fflush(stdout);
        _Exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status))
    {
        printf("FAIL: %s (signal %d)\n", name, WTERMSIG(status));
        test_failures++;
    }
    else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        printf("FAIL: %s\n", name);
        test_failures++;
    }
}

static inline int test_summary(const char* suite_name)
{
    if (test_failures > 0)
    {
        printf("%d %s tests FAILED.\n", test_failures, suite_name);
        return 1;
    }

    printf("All %s tests PASSED.\n", suite_name);
    return 0;
}

#define RUN(test) run_test(#test, test)

#endif //HTTP_SERVER_TEST_HELPERS_H