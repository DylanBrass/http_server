//
// Created by dylanbrass on 2026-10-05.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httpserver.h"
#include "test_helpers.h"

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

static void test_should_return_nonzero_when_same_length_strings_differ(void)
{
    assert(string_compare("cat", "car") != 0);
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

static void test_should_copy_zero_chars_when_max_len_is_one(void)
{
    char dest[16] = "unchanged";
    const int result = string_copy(dest, "hello", 1);
    assert(result == 0);
    assert(get_length(dest) == 0);
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

static void test_should_not_find_when_start_from_exceeds_haystack_length(void)
{
    char* haystack = strdup("abc");
    const StrSearchResult result = find_str_in_str(haystack, "x", 100, 1);
    assert(result.found == false);
    free(haystack);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_find_when_start_from_equals_haystack_length(void)
{
    char* haystack = strdup("abc");
    const StrSearchResult result = find_str_in_str(haystack, "x", 3, 1);
    assert(result.found == false);
    free(haystack);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region string_to_lower

static void test_should_return_negative_one_when_string_to_lower_target_is_null(void)
{
    assert(string_to_lower(nullptr) == -1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_zero_when_string_to_lower_succeeds(void)
{
    char str[] = "Hello";
    assert(string_to_lower(str) == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_keep_empty_string_when_lowering_empty_string(void)
{
    char str[] = "";
    string_to_lower(str);
    assert(get_length(str) == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_lowercase_char_when_string_is_single_uppercase_char(void)
{
    char str[] = "A";
    string_to_lower(str);
    assert(string_compare(str, "a") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_lowercase_all_chars_when_string_is_all_uppercase(void)
{
    char str[] = "HELLO WORLD";
    string_to_lower(str);
    assert(string_compare(str, "hello world") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_change_string_when_already_lowercase(void)
{
    char str[] = "hello";
    string_to_lower(str);
    assert(string_compare(str, "hello") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_lowercase_only_uppercase_when_string_is_mixed_case(void)
{
    char str[] = "HeLLo WoRLd";
    string_to_lower(str);
    assert(string_compare(str, "hello world") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_change_digits_and_punctuation_when_lowering(void)
{
    char str[] = "123 !@#$%^&*()_+-=[]{};':\",./<>?\\|`~";
    constexpr char expected[] = "123 !@#$%^&*()_+-=[]{};':\",./<>?\\|`~";
    string_to_lower(str);
    assert(string_compare(str, expected) == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_convert_only_range_boundaries_when_lowering(void)
{
    char str[] = "@A[Z`a{z";
    string_to_lower(str);
    assert(string_compare(str, "@a[z`a{z") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_preserve_whitespace_when_lowering_http_line(void)
{
    char str[] = "Content-Type: Text/HTML\r\n";
    string_to_lower(str);
    assert(string_compare(str, "content-type: text/html\r\n") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_change_non_ascii_bytes_when_lowering(void)
{
    // "É" in UTF-8 followed by 'A'. Literals are split so \x89 doesn't swallow the 'A'.
    char str[] = "\xC3\x89" "A";
    string_to_lower(str);
    assert(string_compare(str, "\xC3\x89" "a") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_stop_at_null_terminator_when_buffer_has_trailing_data(void)
{
    char buf[] = {'A', 'B', '\0', 'C', 'D', '\0'};
    string_to_lower(buf);
    assert(buf[0] == 'a');
    assert(buf[1] == 'b');
    assert(buf[3] == 'C');
    assert(buf[4] == 'D');
    printf("PASS: %s\n", __func__);
}

static void test_should_be_idempotent_when_lowering_twice(void)
{
    char str[] = "HeLLo";
    string_to_lower(str);
    string_to_lower(str);
    assert(string_compare(str, "hello") == 0);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region span_from_string

static void test_should_create_span_with_string_length_when_creating_span_from_string(void)
{
    const StrSpan span = span_from_string("hello");
    assert(span.length == 5);
    assert(span.data[0] == 'h');
    printf("PASS: %s\n", __func__);
}

static void test_should_create_empty_span_when_creating_span_from_empty_string(void)
{
    const StrSpan span = span_from_string("");
    assert(span.length == 0);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region span_equals

static void test_should_return_true_when_span_matches_and_buffer_continues(void)
{
    const StrSpan span = {"hello world", 5};
    assert(span_equals(span, span_from_string("hello"), CASE_SENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_both_spans_are_empty(void)
{
    const StrSpan span = {"", 0};
    assert(span_equals(span, span_from_string(""), CASE_SENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_first_span_is_shorter(void)
{
    const StrSpan span = {"hell", 4};
    assert(span_equals(span, span_from_string("hello"), CASE_SENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_first_span_is_longer(void)
{
    const StrSpan span = {"hello", 5};
    assert(span_equals(span, span_from_string("hell"), CASE_SENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_case_differs_and_case_sensitive(void)
{
    const StrSpan span = {"Hello", 5};
    assert(span_equals(span, span_from_string("hello"), CASE_SENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_same_length_content_differs_and_case_sensitive(void)
{
    const StrSpan span = {"cat", 3};
    assert(span_equals(span, span_from_string("car"), CASE_SENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_case_differs_and_case_insensitive(void)
{
    const StrSpan span = {"CoNtEnT-TyPe", 12};
    assert(span_equals(span, span_from_string("content-type"), CASE_INSENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_literal_is_uppercase_and_case_insensitive(void)
{
    const StrSpan span = {"close", 5};
    assert(span_equals(span, span_from_string("CLOSE"), CASE_INSENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_buffer_continues_and_case_insensitive(void)
{
    const StrSpan span = {"CLOSEXYZ", 5};
    assert(span_equals(span, span_from_string("close"), CASE_INSENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_two_spans_differ_only_by_case(void)
{
    const StrSpan first = {"Keep-Alive", 10};
    const StrSpan second = {"keep-alive", 10};
    assert(span_equals(first, second, CASE_INSENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_lengths_differ_and_case_insensitive(void)
{
    const StrSpan span = {"Proxy-Connection", 16};
    assert(span_equals(span, span_from_string("connection"), CASE_INSENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_same_length_content_differs_and_case_insensitive(void)
{
    const StrSpan span = {"content-typo", 12};
    assert(span_equals(span, span_from_string("content-type"), CASE_INSENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_treat_chars_next_to_letters_as_equal_when_case_insensitive(void)
{
    const StrSpan span = {"@[", 2};
    assert(span_equals(span, span_from_string("`{"), CASE_INSENSITIVE) == false);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region span_trim

static void test_should_trim_spaces_and_tabs_on_both_sides(void)
{
    const char* text = " \t hi \t ";
    const StrSpan result = span_trim((StrSpan){text, get_length(text)});
    assert(span_equals(result, span_from_string("hi"), CASE_SENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_keep_inner_whitespace_when_trimming(void)
{
    const char* text = "  a b  ";
    const StrSpan result = span_trim((StrSpan){text, get_length(text)});
    assert(span_equals(result, span_from_string("a b"), CASE_SENSITIVE) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_span_when_trimming_only_whitespace(void)
{
    const char* text = " \t  ";
    const StrSpan result = span_trim((StrSpan){text, get_length(text)});
    assert(result.length == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_span_when_trimming_empty_span(void)
{
    const StrSpan result = span_trim((StrSpan){"", 0});
    assert(result.length == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_not_trim_line_breaks(void)
{
    const char* text = "hi\r\n";
    const StrSpan result = span_trim((StrSpan){text, get_length(text)});
    assert(result.length == 4);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region next_delimited

static void test_should_return_each_trimmed_item_when_splitting_on_separator(void)
{
    const char* text = "a, b ,c";
    StrSpan rest = {text, get_length(text)};
    StrSpan item;

    assert(next_delimited(&rest, ',', &item) == true);
    assert(span_equals(item, span_from_string("a"), CASE_SENSITIVE) == true);
    assert(next_delimited(&rest, ',', &item) == true);
    assert(span_equals(item, span_from_string("b"), CASE_SENSITIVE) == true);
    assert(next_delimited(&rest, ',', &item) == true);
    assert(span_equals(item, span_from_string("c"), CASE_SENSITIVE) == true);
    assert(next_delimited(&rest, ',', &item) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_single_item_when_there_is_no_separator(void)
{
    const char* text = "  only  ";
    StrSpan rest = {text, get_length(text)};
    StrSpan item;

    assert(next_delimited(&rest, ';', &item) == true);
    assert(span_equals(item, span_from_string("only"), CASE_SENSITIVE) == true);
    assert(next_delimited(&rest, ';', &item) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_last_item_when_string_ends_with_separator(void)
{
    const char* text = "a,";
    StrSpan rest = {text, get_length(text)};
    StrSpan item;

    assert(next_delimited(&rest, ',', &item) == true);
    assert(span_equals(item, span_from_string("a"), CASE_SENSITIVE) == true);
    assert(next_delimited(&rest, ',', &item) == true);
    assert(item.length == 0);
    assert(next_delimited(&rest, ',', &item) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_one_empty_item_when_span_is_empty(void)
{
    StrSpan rest = {"", 0};
    StrSpan item;

    assert(next_delimited(&rest, ',', &item) == true);
    assert(item.length == 0);
    assert(next_delimited(&rest, ',', &item) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_rest_is_already_consumed(void)
{
    StrSpan rest = {nullptr, 0};
    StrSpan item;

    assert(next_delimited(&rest, ',', &item) == false);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

int main(void)
{
    RUN(test_should_return_zero_when_string_is_empty);
    RUN(test_should_return_one_when_string_is_single_char);
    RUN(test_should_return_length_when_string_has_multiple_chars);

    RUN(test_should_return_zero_when_strings_are_equal);
    RUN(test_should_return_positive_when_first_string_is_longer);
    RUN(test_should_return_negative_when_first_string_is_shorter);
    RUN(test_should_return_positive_when_comparing_nonempty_to_empty);
    RUN(test_should_return_zero_when_both_strings_are_empty);
    RUN(test_should_return_zero_when_both_strings_are_null);
    RUN(test_should_return_negative_when_first_string_is_null);
    RUN(test_should_return_positive_when_second_string_is_null);
    RUN(test_should_return_nonzero_when_same_length_strings_differ);

    RUN(test_should_copy_string_when_it_fits_in_buffer);
    RUN(test_should_copy_empty_string_when_source_is_empty);
    RUN(test_should_return_negative_one_when_source_is_null);
    RUN(test_should_return_negative_one_when_target_is_null);
    RUN(test_should_return_negative_one_when_max_len_is_zero);
    RUN(test_should_truncate_when_source_exceeds_buffer_size);
    RUN(test_should_copy_fully_when_source_exactly_fits_buffer);
    RUN(test_should_copy_zero_chars_when_max_len_is_one);

    RUN(test_should_find_needle_when_present);
    RUN(test_should_not_find_needle_when_absent);
    RUN(test_should_find_second_occurrence_when_requested);
    RUN(test_should_find_tenth_occurrence_when_requested);
    RUN(test_should_not_find_when_occurrence_exceeds_matches);
    RUN(test_should_find_from_start_offset_when_chaining_searches);
    RUN(test_should_not_find_when_target_occurrence_is_zero);
    RUN(test_should_not_find_when_start_from_exceeds_haystack_length);
    RUN(test_should_not_find_when_start_from_equals_haystack_length);

    RUN(test_should_return_negative_one_when_string_to_lower_target_is_null);
    RUN(test_should_return_zero_when_string_to_lower_succeeds);
    RUN(test_should_keep_empty_string_when_lowering_empty_string);
    RUN(test_should_lowercase_char_when_string_is_single_uppercase_char);
    RUN(test_should_lowercase_all_chars_when_string_is_all_uppercase);
    RUN(test_should_not_change_string_when_already_lowercase);
    RUN(test_should_lowercase_only_uppercase_when_string_is_mixed_case);
    RUN(test_should_not_change_digits_and_punctuation_when_lowering);
    RUN(test_should_convert_only_range_boundaries_when_lowering);
    RUN(test_should_preserve_whitespace_when_lowering_http_line);
    RUN(test_should_not_change_non_ascii_bytes_when_lowering);
    RUN(test_should_stop_at_null_terminator_when_buffer_has_trailing_data);
    RUN(test_should_be_idempotent_when_lowering_twice);




    RUN(test_should_create_span_with_string_length_when_creating_span_from_string);
    RUN(test_should_create_empty_span_when_creating_span_from_empty_string);

    RUN(test_should_return_true_when_span_matches_and_buffer_continues);
    RUN(test_should_return_true_when_both_spans_are_empty);
    RUN(test_should_return_false_when_first_span_is_shorter);
    RUN(test_should_return_false_when_first_span_is_longer);
    RUN(test_should_return_false_when_case_differs_and_case_sensitive);
    RUN(test_should_return_false_when_same_length_content_differs_and_case_sensitive);
    RUN(test_should_return_true_when_case_differs_and_case_insensitive);
    RUN(test_should_return_true_when_literal_is_uppercase_and_case_insensitive);
    RUN(test_should_return_true_when_buffer_continues_and_case_insensitive);
    RUN(test_should_return_true_when_two_spans_differ_only_by_case);
    RUN(test_should_return_false_when_lengths_differ_and_case_insensitive);
    RUN(test_should_return_false_when_same_length_content_differs_and_case_insensitive);
    RUN(test_should_not_treat_chars_next_to_letters_as_equal_when_case_insensitive);

    RUN(test_should_trim_spaces_and_tabs_on_both_sides);
    RUN(test_should_keep_inner_whitespace_when_trimming);
    RUN(test_should_return_empty_span_when_trimming_only_whitespace);
    RUN(test_should_return_empty_span_when_trimming_empty_span);
    RUN(test_should_not_trim_line_breaks);

    RUN(test_should_return_each_trimmed_item_when_splitting_on_separator);
    RUN(test_should_return_single_item_when_there_is_no_separator);
    RUN(test_should_return_empty_last_item_when_string_ends_with_separator);
    RUN(test_should_return_one_empty_item_when_span_is_empty);
    RUN(test_should_return_false_when_rest_is_already_consumed);

    return test_summary("str_functions");
}