//
// Created by dylanbrass on 2026-09-06.
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "httpserver.h"
#define DEFAULT_ROUTE_LIMIT 5

static Route* routes = nullptr;
static size_t route_capacity = 0;
static size_t route_count = 0;

int register_route(const enum HTTP_METHOD http_method, const enum CONTENT_TYPE request_body_content_type,
                   const char* uri, const RouteHandler handler)
{
    if (route_count == route_capacity)
    {
        const size_t new_capacity = route_capacity == 0 ? DEFAULT_ROUTE_LIMIT : route_capacity * 2;
        Route* new_routes = realloc(routes, new_capacity * sizeof(Route));

        if (new_routes == nullptr)
        {
            return -1;
        }

        // printf("New capacity %d\n", new_capacity);
        routes = new_routes;
        route_capacity = new_capacity;
    }

    Route* new_route = &routes[route_count];

    new_route->handler = handler;
    string_copy(new_route->uri, uri, URI_MAX_LENGTH);
    new_route->http_method = http_method;
    new_route->request_body_content_type = request_body_content_type;

    route_count++;
    return 0;
}

RouteHandler find_route(const enum HTTP_METHOD http_method, const char* uri)
{
    for (size_t i = 0; i < route_count; i++)
    {
        if (http_method == routes[i].http_method && string_compare(uri, routes[i].uri) == 0)
        {
            return routes[i].handler;
        }
    }
    return nullptr;
}

static const char* find_crlf(const char* str)
{
    for (; *str != '\0'; str++)
    {
        if (str[0] == '\r' && str[1] == '\n') return str;
    }
    return nullptr;
}

static const char* skip_leading_crlf(const char* buffer)
{
    while (buffer[0] == '\r' && buffer[1] == '\n')
    {
        buffer += 2;
    }
    return buffer;
}

static bool split_request_line(const char* buffer, RequestLine* request_line)
{
    const char* line_start = skip_leading_crlf(buffer);
    const char* line_end = find_crlf(line_start);
    if (line_end == nullptr) return false;

    const char* first_space = line_start;
    while (first_space < line_end && *first_space != ' ') first_space++;
    if (first_space == line_start || first_space == line_end) return false;

    const char* uri_start = first_space + 1;
    const char* second_space = uri_start;
    while (second_space < line_end && *second_space != ' ') second_space++;
    if (second_space == uri_start || second_space == line_end) return false;

    const char* version_start = second_space + 1;
    if (version_start == line_end) return false;

    for (const char* c = version_start; c < line_end; c++)
    {
        if (*c == ' ') return false;
    }

    request_line->method = (StrSpan){line_start, (size_t)(first_space - line_start)};
    request_line->uri = (StrSpan){uri_start, (size_t)(second_space - uri_start)};
    request_line->version = (StrSpan){version_start, (size_t)(line_end - version_start)};
    return true;
}

static const char* headers_start(const char* buffer)
{
    const char* line_end = find_crlf(skip_leading_crlf(buffer));
    return line_end == nullptr ? nullptr : line_end + 2;
}

static bool next_header(const char** cursor, StrSpan* name, StrSpan* value)
{
    const char* line = *cursor;

    while (line != nullptr && line[0] != '\0' && !(line[0] == '\r' && line[1] == '\n'))
    {
        const char* line_end = find_crlf(line);
        if (line_end == nullptr) break;

        const char* colon = line;
        while (colon < line_end && *colon != ':') colon++;

        const char* next_line = line_end + 2;

        if (colon != line && colon != line_end)
        {
            *name = (StrSpan){line, (size_t)(colon - line)};
            *value = span_trim((StrSpan){colon + 1, (size_t)(line_end - colon - 1)});
            *cursor = next_line;
            return true;
        }

        line = next_line;
    }

    *cursor = nullptr;
    return false;
}

static HeaderLookup lookup_header(const char* buffer, const char* header_name)
{
    HeaderLookup result = {0};
    const char* cursor = headers_start(buffer);
    StrSpan name;
    StrSpan value;

    while (next_header(&cursor, &name, &value))
    {
        if (!span_equals(name, span_from_string(header_name), CASE_INSENSITIVE)) continue;

        if (result.count == 0)
        {
            result.value = value;
        }
        else if (!span_equals(result.value, value, CASE_INSENSITIVE))
        {
            result.conflict = true;
        }
        result.count++;
    }
    return result;
}

enum HTTP_METHOD parse_http_method(const char* buffer)
{
    RequestLine request_line;
    if (!split_request_line(buffer, &request_line)) return UNKNOWN;

    // the method is case-sensitive
    if (span_equals(request_line.method, span_from_string("GET"), CASE_SENSITIVE)) return GET;
    if (span_equals(request_line.method, span_from_string("POST"), CASE_SENSITIVE)) return POST;
    if (span_equals(request_line.method, span_from_string("PUT"), CASE_SENSITIVE)) return PUT;
    if (span_equals(request_line.method, span_from_string("DELETE"), CASE_SENSITIVE)) return DELETE;

    return UNKNOWN;
}

void parse_uri(const char* buffer, char* uri_out)
{
    uri_out[0] = '\0';

    RequestLine request_line;
    if (!split_request_line(buffer, &request_line)) return;

    if (request_line.uri.length >= URI_MAX_LENGTH) return;

    string_copy(uri_out, request_line.uri.data, request_line.uri.length + 1);
}

bool uri_too_long(const char* buffer)
{
    RequestLine request_line;
    if (!split_request_line(buffer, &request_line)) return false;

    return request_line.uri.length >= URI_MAX_LENGTH;
}

enum CONTENT_TYPE parse_content_type(const char* buffer)
{
    const HeaderLookup header = lookup_header(buffer, HEADER_CONTENT_TYPE);

    if (header.count == 0 || header.conflict) return CONTENT_TYPE_NONE;

    StrSpan rest = header.value;
    StrSpan media_type;
    next_delimited(&rest, ';', &media_type);

    bool has_utf8_charset = false;
    StrSpan parameter;
    while (next_delimited(&rest, ';', &parameter))
    {
        if (span_equals(parameter, span_from_string("charset=utf-8"), CASE_INSENSITIVE)) has_utf8_charset = true;
    }

    // media types are case-insensitive (RFC 9110 section 8.3.1)
    if (span_equals(media_type, span_from_string("text/html"), CASE_INSENSITIVE)) return TEXT_HTML;
    if (span_equals(media_type, span_from_string("application/json"), CASE_INSENSITIVE))
    {
        return has_utf8_charset ? APPLICATION_JSON_UTF8 : APPLICATION_JSON;
    }
    if (span_equals(media_type, span_from_string("application/xml"), CASE_INSENSITIVE)) return APPLICATION_XML;
    if (span_equals(media_type, span_from_string("image/jpeg"), CASE_INSENSITIVE)) return IMAGE_JPEG;
    if (span_equals(media_type, span_from_string("image/png"), CASE_INSENSITIVE)) return IMAGE_PNG;

    return CONTENT_TYPE_NONE;
}

ContentLengthResult parse_content_length(const char* buffer)
{
    const HeaderLookup header = lookup_header(buffer, HEADER_CONTENT_LENGTH);

    // If we do not see the header, this means there is no body
    if (header.count == 0)
    {
        return (ContentLengthResult){0, true};
    }

    // two different lengths, or a header with an empty value
    if (header.conflict || header.value.length == 0)
    {
        return (ContentLengthResult){0, false};
    }

    size_t result = 0;

    for (size_t i = 0; i < header.value.length; i++)
    {
        const char c = header.value.data[i];

        if (c < '0' || c > '9')
        {
            return (ContentLengthResult){0, false};
        }

        // Do - '0' (48), so that the number will be given
        // For example 8 is 56 and 0 is 48, 56 - 48 = 8
        const size_t digit = (size_t)(c - '0');

        if (result > (SIZE_MAX - digit) / 10)
        {
            return (ContentLengthResult){0, false};
        }

        result = result * 10 + digit;
    }

    return (ContentLengthResult){result, true};
}

enum HTTP_VERSION parse_http_version(const char* buffer)
{
    RequestLine request_line;
    if (!split_request_line(buffer, &request_line)) return HTTP_VERSION_UNKNOWN;

    // the protocol name is case-sensitive, http/1.1 is not valid
    if (span_equals(request_line.version, span_from_string("HTTP/1.1"), CASE_SENSITIVE)) return HTTP_1_1;
    if (span_equals(request_line.version, span_from_string("HTTP/1.0"), CASE_SENSITIVE)) return HTTP_1_0;

    return HTTP_VERSION_UNKNOWN;
}

bool parse_keep_alive(const char* buffer)
{
    const enum HTTP_VERSION version = parse_http_version(buffer);

    bool has_close = false;
    bool has_keep_alive = false;

    const char* cursor = headers_start(buffer);
    StrSpan name;
    StrSpan value;

    while (next_header(&cursor, &name, &value))
    {
        if (!span_equals(name, span_from_string(HEADER_CONNECTION), CASE_INSENSITIVE)) continue;

        StrSpan rest = value;
        StrSpan option;
        while (next_delimited(&rest, ',', &option))
        {
            if (span_equals(option, span_from_string("close"), CASE_INSENSITIVE)) has_close = true;
            if (span_equals(option, span_from_string("keep-alive"), CASE_INSENSITIVE)) has_keep_alive = true;
        }
    }

    if (has_close) return false;
    if (has_keep_alive) return true;

    return version == HTTP_1_1;
}

void route_cleanup()
{
    free(routes);
    route_capacity = 0;
    route_count = 0;
    routes = nullptr;
}