//
// Created by dylanbrass on 2026-09-06.
//

#include <stddef.h>

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
    size_t i = 0;

    while (i < max_len - 1 && source[i] != '\0')
    {
        target[i] = source[i];
        ++i;
    }

    target[i] = '\0';

    return 0;
}

int find_str_in_str(const char* haystack, const char* needle, const size_t target_occurrence)
{
    const size_t needle_len = get_length(needle);
    size_t start = 0;
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
                //return position of the start of the needle in the pattern
                return (int)start;
            }
        }

        start++;
    }

    return -1;
}
