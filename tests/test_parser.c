//
// Created by dylanbrass on 2026-10-07.
//
#include <assert.h>
#include <stdio.h>

#include "httpserver.h"
#include "test_helpers.h"

#pragma region parse_http_method

static void test_should_return_unknown_when_method_exceeds_buffer_size(void)
{
    char* req = make_request("", 1000, 'A', " /test HTTP/1.1\r\n\r\n");
    assert(parse_http_method(req) == UNKNOWN);
    free(req);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_get_when_method_is_get(void)
{
    assert(parse_http_method("GET /test HTTP/1.1\r\n\r\n") == GET);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_post_when_method_is_post(void)
{
    assert(parse_http_method("POST /test HTTP/1.1\r\n\r\n") == POST);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_put_when_method_is_put(void)
{
    assert(parse_http_method("PUT /test HTTP/1.1\r\n\r\n") == PUT);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_delete_when_method_is_delete(void)
{
    assert(parse_http_method("DELETE /test HTTP/1.1\r\n\r\n") == DELETE);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_method_is_unrecognized(void)
{
    assert(parse_http_method("PATCH /test HTTP/1.1\r\n\r\n") == UNKNOWN);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_no_space_in_request(void)
{
    assert(parse_http_method("GARBAGEWITHNOSPACEATALL") == UNKNOWN);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_method_case_does_not_match(void)
{
    assert(parse_http_method("get /test HTTP/1.1\r\n\r\n") == UNKNOWN);
    printf("PASS: %s\n", __func__);
}

static void test_should_ignore_leading_crlf_when_parsing_method(void)
{
    assert(parse_http_method("\r\nGET /test HTTP/1.1\r\n\r\n") == GET);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region parse_content_type

static void test_should_return_none_when_content_type_exceeds_buffer_size(void)
{
    char* req = make_request("GET /test HTTP/1.1\r\nContent-Type: ", 5000, 'x', "\r\n\r\n");
    assert(parse_content_type(req) == CONTENT_TYPE_NONE);
    free(req);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_application_json_when_content_type_is_json(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: application/json\r\n\r\n";
    assert(parse_content_type(req) == APPLICATION_JSON);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_text_html_when_content_type_is_html(void)
{
    const char* req = "GET /test HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_none_when_content_type_header_is_absent(void)
{
    assert(parse_content_type("GET /test HTTP/1.1\r\n\r\n") == CONTENT_TYPE_NONE);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_json_utf8_when_content_type_has_charset(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: application/json; charset=utf-8\r\n\r\n";
    assert(parse_content_type(req) == APPLICATION_JSON_UTF8);
    printf("PASS: %s\n", __func__);
}

static void test_should_match_content_type_when_header_name_is_lowercase(void)
{
    const char* req = "POST /test HTTP/1.1\r\ncontent-type: text/html\r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_match_content_type_when_header_name_is_uppercase(void)
{
    const char* req = "POST /test HTTP/1.1\r\nCONTENT-TYPE: text/html\r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_match_content_type_when_media_type_case_differs(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: Application/JSON\r\n\r\n";
    assert(parse_content_type(req) == APPLICATION_JSON);
    printf("PASS: %s\n", __func__);
}

static void test_should_match_content_type_when_no_space_after_colon(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type:text/html\r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_trim_whitespace_when_content_type_has_extra_ows(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type:   text/html  \r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_json_utf8_when_charset_has_no_space(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: application/json;charset=utf-8\r\n\r\n";
    assert(parse_content_type(req) == APPLICATION_JSON_UTF8);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_json_utf8_when_charset_is_uppercase(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
    assert(parse_content_type(req) == APPLICATION_JSON_UTF8);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_text_html_when_html_has_charset_parameter(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
    assert(parse_content_type(req) == TEXT_HTML);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_none_when_content_type_is_only_in_body(void)
{
    const char* req = "POST /test HTTP/1.1\r\n\r\nContent-Type: text/html\r\n";
    assert(parse_content_type(req) == CONTENT_TYPE_NONE);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_none_when_header_name_only_ends_with_content_type(void)
{
    const char* req = "POST /test HTTP/1.1\r\nX-Original-Content-Type: text/html\r\n\r\n";
    assert(parse_content_type(req) == CONTENT_TYPE_NONE);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_none_when_content_type_is_duplicated_with_conflict(void)
{
    const char* req = "POST /test HTTP/1.1\r\nContent-Type: text/html\r\nContent-Type: application/json\r\n\r\n";
    assert(parse_content_type(req) == CONTENT_TYPE_NONE);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region parse_http_version

static void test_should_return_unknown_when_version_exceeds_buffer_size(void)
{
    char* req = make_request("GET /test ", 1000, 'Z', "\r\n\r\n");
    assert(parse_http_version(req) == HTTP_VERSION_UNKNOWN);
    free(req);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_http_1_1_when_version_is_1_1(void)
{
    assert(parse_http_version("GET /test HTTP/1.1\r\n\r\n") == HTTP_1_1);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_http_1_0_when_version_is_1_0(void)
{
    assert(parse_http_version("GET /test HTTP/1.0\r\n\r\n") == HTTP_1_0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_version_is_unrecognized(void)
{
    assert(parse_http_version("GET /test HTTP/2.0\r\n\r\n") == HTTP_VERSION_UNKNOWN);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_no_second_space_in_request(void)
{
    assert(parse_http_version("GET /test\r\n\r\n") == HTTP_VERSION_UNKNOWN);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_unknown_when_version_comes_from_a_header_line(void)
{
    assert(parse_http_version("GET /test\r\nX: HTTP/1.1\r\n\r\n") == HTTP_VERSION_UNKNOWN);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region parse_keep_alive

static void test_should_not_crash_when_connection_header_exceeds_buffer_size(void)
{
    char* req = make_request("GET /test HTTP/1.1\r\nConnection: ", 5000, 'y', "\r\n\r\n");
    const bool result = parse_keep_alive(req);
    (void)result;
    free(req);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_connection_is_keep_alive(void)
{
    const char* req = "GET /test HTTP/1.0\r\nConnection: keep-alive\r\n\r\n";
    assert(parse_keep_alive(req) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_connection_is_close(void)
{
    const char* req = "GET /test HTTP/1.1\r\nConnection: close\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_default_to_true_when_connection_header_is_absent_on_http_1_1(void)
{
    assert(parse_keep_alive("GET /test HTTP/1.1\r\n\r\n") == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_default_to_false_when_connection_header_is_absent_on_http_1_0(void)
{
    assert(parse_keep_alive("GET /test HTTP/1.0\r\n\r\n") == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_keep_alive_value_case_differs(void)
{
    const char* req = "GET /test HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n";
    assert(parse_keep_alive(req) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_connection_header_name_is_lowercase(void)
{
    const char* req = "GET /test HTTP/1.1\r\nconnection: close\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_connection_header_name_is_uppercase(void)
{
    const char* req = "GET /test HTTP/1.1\r\nCONNECTION: close\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_close_has_no_space_after_colon(void)
{
    const char* req = "GET /test HTTP/1.1\r\nConnection:close\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_close_has_trailing_whitespace(void)
{
    const char* req = "GET /test HTTP/1.1\r\nConnection: close  \r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_close_is_in_connection_list(void)
{
    const char* req = "GET /test HTTP/1.1\r\nConnection: close, TE\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_true_when_keep_alive_is_in_connection_list_on_http_1_0(void)
{
    const char* req = "GET /test HTTP/1.0\r\nConnection: keep-alive, Upgrade\r\n\r\n";
    assert(parse_keep_alive(req) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_false_when_close_and_keep_alive_are_both_listed(void)
{
    const char* req = "GET /test HTTP/1.1\r\nConnection: keep-alive, close\r\n\r\n";
    assert(parse_keep_alive(req) == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_ignore_connection_header_when_it_is_only_in_body(void)
{
    const char* req = "POST /test HTTP/1.1\r\n\r\nConnection: close\r\n";
    assert(parse_keep_alive(req) == true);
    printf("PASS: %s\n", __func__);
}

static void test_should_ignore_header_when_name_only_ends_with_connection(void)
{
    const char* req = "GET /test HTTP/1.1\r\nProxy-Connection: close\r\n\r\n";
    assert(parse_keep_alive(req) == true);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region parse_uri

static void test_should_extract_uri_when_request_is_well_formed(void)
{
    char uri[URI_MAX_LENGTH];
    parse_uri("GET /test HTTP/1.1\r\n\r\n", uri);
    assert(string_compare(uri, "/test") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_when_request_is_malformed(void)
{
    char uri[URI_MAX_LENGTH];
    parse_uri("GET /test", uri);
    assert(string_compare(uri, "") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_uri_when_request_line_has_no_version(void)
{
    char uri[URI_MAX_LENGTH];
    parse_uri("GET /test\r\nX: y\r\n\r\n", uri);
    assert(string_compare(uri, "") == 0);
    printf("PASS: %s\n", __func__);
}

static void test_should_return_empty_when_uri_exceeds_buffer_size(void)
{
    char* req = make_request("GET /", 1000, 'a', " HTTP/1.1\r\n\r\n");
    char uri[URI_MAX_LENGTH];
    parse_uri(req, uri);
    assert(get_length(uri) == 0);
    free(req);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

#pragma region parse_content_length

static void test_should_be_invalid_when_content_length_has_trailing_garbage(void)
{
    const ContentLengthResult r = parse_content_length("POST / HTTP/1.1\r\nContent-Length: 12abc\r\n\r\n");
    assert(r.is_valid == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_parse_value_when_content_length_name_is_lowercase(void)
{
    const ContentLengthResult r = parse_content_length("POST / HTTP/1.1\r\ncontent-length: 5\r\n\r\n");
    assert(r.is_valid == true);
    assert(r.value == 5);
    printf("PASS: %s\n", __func__);
}

static void test_should_parse_value_when_content_length_has_no_space_after_colon(void)
{
    const ContentLengthResult r = parse_content_length("POST / HTTP/1.1\r\nContent-Length:5\r\n\r\n");
    assert(r.is_valid == true);
    assert(r.value == 5);
    printf("PASS: %s\n", __func__);
}

static void test_should_parse_value_when_content_length_has_extra_leading_spaces(void)
{
    const ContentLengthResult r = parse_content_length("POST / HTTP/1.1\r\nContent-Length:   5\r\n\r\n");
    assert(r.is_valid == true);
    assert(r.value == 5);
    printf("PASS: %s\n", __func__);
}

static void test_should_be_invalid_when_content_length_is_duplicated_with_conflict(void)
{
    const ContentLengthResult r = parse_content_length(
        "POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 10\r\n\r\n");
    assert(r.is_valid == false);
    printf("PASS: %s\n", __func__);
}

static void test_should_ignore_content_length_when_it_is_only_in_body(void)
{
    const ContentLengthResult r = parse_content_length("POST / HTTP/1.1\r\n\r\nContent-Length: 99\r\n");
    assert(r.value == 0);
    printf("PASS: %s\n", __func__);
}

#pragma endregion

int main(void)
{
    RUN(test_should_return_unknown_when_method_exceeds_buffer_size);
    RUN(test_should_return_get_when_method_is_get);
    RUN(test_should_return_post_when_method_is_post);
    RUN(test_should_return_put_when_method_is_put);
    RUN(test_should_return_delete_when_method_is_delete);
    RUN(test_should_return_unknown_when_method_is_unrecognized);
    RUN(test_should_return_unknown_when_no_space_in_request);
    RUN(test_should_return_unknown_when_method_case_does_not_match);
    RUN(test_should_ignore_leading_crlf_when_parsing_method);

    RUN(test_should_return_none_when_content_type_exceeds_buffer_size);
    RUN(test_should_return_application_json_when_content_type_is_json);
    RUN(test_should_return_text_html_when_content_type_is_html);
    RUN(test_should_return_none_when_content_type_header_is_absent);
    RUN(test_should_return_json_utf8_when_content_type_has_charset);
    RUN(test_should_match_content_type_when_header_name_is_lowercase);
    RUN(test_should_match_content_type_when_header_name_is_uppercase);
    RUN(test_should_match_content_type_when_media_type_case_differs);
    RUN(test_should_match_content_type_when_no_space_after_colon);
    RUN(test_should_trim_whitespace_when_content_type_has_extra_ows);
    RUN(test_should_return_json_utf8_when_charset_has_no_space);
    RUN(test_should_return_json_utf8_when_charset_is_uppercase);
    RUN(test_should_return_text_html_when_html_has_charset_parameter);
    RUN(test_should_return_none_when_content_type_is_only_in_body);
    RUN(test_should_return_none_when_header_name_only_ends_with_content_type);
    RUN(test_should_return_none_when_content_type_is_duplicated_with_conflict);

    RUN(test_should_return_unknown_when_version_exceeds_buffer_size);
    RUN(test_should_return_http_1_1_when_version_is_1_1);
    RUN(test_should_return_http_1_0_when_version_is_1_0);
    RUN(test_should_return_unknown_when_version_is_unrecognized);
    RUN(test_should_return_unknown_when_no_second_space_in_request);
    RUN(test_should_return_unknown_when_version_comes_from_a_header_line);

    RUN(test_should_not_crash_when_connection_header_exceeds_buffer_size);
    RUN(test_should_return_true_when_connection_is_keep_alive);
    RUN(test_should_return_false_when_connection_is_close);
    RUN(test_should_default_to_true_when_connection_header_is_absent_on_http_1_1);
    RUN(test_should_default_to_false_when_connection_header_is_absent_on_http_1_0);
    RUN(test_should_return_true_when_keep_alive_value_case_differs);
    RUN(test_should_return_false_when_connection_header_name_is_lowercase);
    RUN(test_should_return_false_when_connection_header_name_is_uppercase);
    RUN(test_should_return_false_when_close_has_no_space_after_colon);
    RUN(test_should_return_false_when_close_has_trailing_whitespace);
    RUN(test_should_return_false_when_close_is_in_connection_list);
    RUN(test_should_return_true_when_keep_alive_is_in_connection_list_on_http_1_0);
    RUN(test_should_return_false_when_close_and_keep_alive_are_both_listed);
    RUN(test_should_ignore_connection_header_when_it_is_only_in_body);
    RUN(test_should_ignore_header_when_name_only_ends_with_connection);

    RUN(test_should_extract_uri_when_request_is_well_formed);
    RUN(test_should_return_empty_when_request_is_malformed);
    RUN(test_should_return_empty_uri_when_request_line_has_no_version);
    RUN(test_should_return_empty_when_uri_exceeds_buffer_size);

    RUN(test_should_be_invalid_when_content_length_has_trailing_garbage);
    RUN(test_should_parse_value_when_content_length_name_is_lowercase);
    RUN(test_should_parse_value_when_content_length_has_no_space_after_colon);
    RUN(test_should_parse_value_when_content_length_has_extra_leading_spaces);
    RUN(test_should_be_invalid_when_content_length_is_duplicated_with_conflict);
    RUN(test_should_ignore_content_length_when_it_is_only_in_body);

    return test_summary("parser");
}