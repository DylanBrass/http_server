//
// Created by dylanbrass on 2026-10-05.
//
#include <assert.h>
#include <stdio.h>

#include "httpserver.h"

#pragma region get_length

static void test_should_return_zero_when_string_is_empty(void)
{
    assert(get_length("") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_one_when_string_is_single_char(void)
{
    assert(get_length("a") == 1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_length_when_string_has_multiple_chars(void)
{
    assert(get_length("hello") == 5);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region string_compare

static void test_should_return_zero_when_strings_are_equal(void)
{
    assert(string_compare("test", "test") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_positive_when_first_string_is_longer(void)
{
    assert(string_compare("wrong", "test") == 1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_negative_when_first_string_is_shorter(void)
{
    assert(string_compare("test", "wrong") == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_positive_when_comparing_nonempty_to_empty(void)
{
    assert(string_compare("wrong", "") == 1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_zero_when_both_strings_are_empty(void)
{
    assert(string_compare("", "") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_zero_when_both_strings_are_null(void)
{
    assert(string_compare(nullptr, nullptr) == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_negative_when_first_string_is_null(void)
{
    assert(string_compare(nullptr, "nullptr") == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_positive_when_second_string_is_null(void)
{
    assert(string_compare("nullptr", nullptr) == 1);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region string_copy

static void test_should_copy_string_when_it_fits_in_buffer(void)
{
    char dest[16];
    string_copy(dest, "hello", sizeof(dest));
    assert(string_compare(dest, "hello") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_copy_empty_string_when_source_is_empty(void)
{
    char dest[16];
    string_copy(dest, "", sizeof(dest));
    assert(string_compare(dest, "") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_negative_one_when_source_is_null(void)
{
    char dest[16];
    const int result = string_copy(dest, nullptr, sizeof(dest));
    assert(result == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_negative_one_when_target_is_null(void)
{
    const int result = string_copy(nullptr, "Test", 16);
    assert(result == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_negative_one_when_max_len_is_zero(void)
{
    char dest[16];
    const int result = string_copy(dest, "Test", 0);
    assert(result == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_truncate_when_source_exceeds_buffer_size(void)
{
    char small_dest[4];
    string_copy(small_dest, "hello world", sizeof(small_dest));
    assert(get_length(small_dest) == 3);
    assert(string_compare(small_dest, "hel") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_copy_fully_when_source_exactly_fits_buffer(void)
{
    char exact_dest[6];
    string_copy(exact_dest, "hello", sizeof(exact_dest));
    assert(get_length(exact_dest) == 5);
    assert(string_compare(exact_dest, "hello") == 0);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region find_str_in_str

static void test_should_find_needle_when_present(void)
{
    const StrSearchResult result = find_str_in_str("GET /test HTTP/1.1", " ", 0, 1);
    assert(result.found == true);
    assert(result.position == 3);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_find_needle_when_absent(void)
{
    const StrSearchResult result = find_str_in_str("GET /test HTTP/1.1", "XYZ", 0, 1);
    assert(result.found == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_find_second_occurrence_when_requested(void)
{
    const StrSearchResult result = find_str_in_str("a.b.c.d", ".", 0, 2);
    assert(result.found == true);
    assert(result.position == 3);
    printf("PASS: %s\n", __func__);
}

static void test_should_find_tenth_occurrence_when_requested(void)
{
    const StrSearchResult result = find_str_in_str("1.2.3.4.5.6.7.8.9.10.11.12.13.14.15", ".", 0, 10);
    assert(result.found == true);
    assert(result.position == 20);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_find_when_occurrence_exceeds_matches(void)
{
    const StrSearchResult result = find_str_in_str("a.b.c.d", ".", 0, 10);
    assert(result.found == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_find_from_start_offset_when_chaining_searches(void)
{
    const StrSearchResult result = find_str_in_str("GET /test HTTP/1.1", " ", 4, 1);
    assert(result.found == true);
    assert(result.position == 9);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_find_when_target_occurrence_is_zero(void)
{
    const StrSearchResult result = find_str_in_str("a.b.c.d", ".", 0, 0);
    assert(result.found == false);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

int main(void)
{
    test_should_return_zero_when_string_is_empty();
    test_should_return_one_when_string_is_single_char();
    test_should_return_length_when_string_has_multiple_chars();

    test_should_return_zero_when_strings_are_equal();
    test_should_return_positive_when_first_string_is_longer();
    test_should_return_negative_when_first_string_is_shorter();
    test_should_return_positive_when_comparing_nonempty_to_empty();
    test_should_return_zero_when_both_strings_are_empty();
    test_should_return_zero_when_both_strings_are_null();
    test_should_return_negative_when_first_string_is_null();
    test_should_return_positive_when_second_string_is_null();

    test_should_copy_string_when_it_fits_in_buffer();
    test_should_copy_empty_string_when_source_is_empty();
    test_should_return_negative_one_when_source_is_null();
    test_should_return_negative_one_when_target_is_null();
    test_should_return_negative_one_when_max_len_is_zero();
    test_should_truncate_when_source_exceeds_buffer_size();
    test_should_copy_fully_when_source_exactly_fits_buffer();

    test_should_find_needle_when_present();
    test_should_not_find_needle_when_absent();
    test_should_find_second_occurrence_when_requested();
    test_should_find_tenth_occurrence_when_requested();
    test_should_not_find_when_occurrence_exceeds_matches();
    test_should_find_from_start_offset_when_chaining_searches();
    test_should_not_find_when_target_occurrence_is_zero();

    printf("All str_functions tests PASSED.\n");
    return 0;
}