//
// Created by dylanbrass on 2026-09-06.
//

#include <stddef.h>

#include "httpserver.h"

size_t get_length(const char* arr)
{
    size_t count = 0;
    while (arr[count] != '\0')
    {
        count++;
    }
    return count;
}

int string_compare(const char* str1, const char* str2)
{
    if (str1 == nullptr && str2 == nullptr) return 0;
    if (str1 == nullptr || str2 == nullptr) return str1 == nullptr ? -1 : 1;

    const size_t str1_len = get_length(str1);
    const size_t str2_len = get_length(str2);
    size_t i = 0;

    if (str1_len != str2_len)
    {
        return str1_len < str2_len ? -1 : 1;
    }

    for (i = 0; i < str1_len; i++)
    {
        if (str1[i] != str2[i])
        {
            return str1[i] - str2[i];
        }
    }

    return 0;
}

int string_copy(char* target, const char* source, const size_t max_len)
{
    if (source == nullptr) return -1;
    if (target == nullptr) return -1;
    if (max_len == 0) return -1;

    size_t i = 0;

    while (i < max_len - 1 && source[i] != '\0')
    {
        target[i] = source[i];
        ++i;
    }

    target[i] = '\0';

    return 0;
}

StrSearchResult find_str_in_str(const char* haystack, const char* needle, const size_t start_from,
                                const size_t target_occurrence)
{
    const size_t haystack_len = get_length(haystack);
    if (start_from > haystack_len)
    {
        return (StrSearchResult){0, false};
    }

    const size_t needle_len = get_length(needle);
    size_t start = start_from;
    size_t nb_found = 0;

    while (haystack[start] != '\0')
    {
        size_t j = 0;

        while (j < needle_len && haystack[start + j] == needle[j])
        {
            j++;
        }

        if (j == needle_len)
        {
            ++nb_found;

            if (nb_found == target_occurrence)
            {
                return (StrSearchResult){start, true};
            }
        }

        start++;
    }

    return (StrSearchResult){0, false};
}

int string_to_lower(char *str)
{
    if (str == nullptr)
    {
        return -1;
    }

    for (; *str != '\0'; str++)
    {
        if (*str >= 'A' && *str <= 'Z')
        {
            *str = (char)(*str + ('a' - 'A'));
        }
    }
    return 0;
}

char char_to_lower(const char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

StrSpan span_from_string(const char* str)
{
    return (StrSpan){str, get_length(str)};
}

bool span_equals(const StrSpan first, const StrSpan second, const enum CASE_SENSITIVITY case_sensitivity)
{
    if (first.length != second.length) return false;

    for (size_t i = 0; i < first.length; i++)
    {
        char first_char = first.data[i];
        char second_char = second.data[i];

        if (case_sensitivity == CASE_INSENSITIVE)
        {
            first_char = char_to_lower(first_char);
            second_char = char_to_lower(second_char);
        }

        if (first_char != second_char) return false;
    }
    return true;
}

StrSpan span_trim(StrSpan span)
{
    while (span.length > 0 && (span.data[0] == ' ' || span.data[0] == '\t'))
    {
        span.data++;
        span.length--;
    }

    while (span.length > 0 && (span.data[span.length - 1] == ' ' || span.data[span.length - 1] == '\t'))
    {
        span.length--;
    }
    return span;
}

bool next_delimited(StrSpan* rest, const char separator, StrSpan* item)
{
    if (rest->data == nullptr) return false;

    size_t end = 0;
    while (end < rest->length && rest->data[end] != separator)
    {
        end++;
    }

    *item = span_trim((StrSpan){rest->data, end});

    if (end < rest->length)
    {
        rest->data += end + 1;
        rest->length -= end + 1;
    }
    else
    {
        rest->data = nullptr;
        rest->length = 0;
    }
    return true;
}