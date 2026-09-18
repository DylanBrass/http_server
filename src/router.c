//
// Created by dylanbrass on 2026-09-06.
//
#include <stdio.h>
#include <stdlib.h>

#include "httpserver.h"
#define DEFAULT_ROUTE_LIMIT 5

static Route* routes = nullptr;
static int route_capacity = 0;
static int route_count = 0;

int register_route(const enum HTTP_METHOD http_method, const enum CONTENT_TYPE request_body_content_type,
                   const char* uri, const RouteHandler handler)
{
    if (route_count == route_capacity)
    {
        const int new_capacity = route_capacity == 0 ? DEFAULT_ROUTE_LIMIT : route_capacity * 2;
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

RouteHandler find_route(enum HTTP_METHOD http_method, char* uri)
{
    for (int i = 0; i < route_count; i++)
    {
        if (http_method == routes[i].http_method && string_compare(uri, routes[i].uri) == 0)
        {
            return routes[i].handler;
        }
    }
    return nullptr;
}

enum HTTP_METHOD parse_http_method(const char* buffer)
{
    const int space_pos = find_str_in_str(buffer, " ", 0, 1);
    if (space_pos == -1) return UNKNOWN;

    char method_str[16];

    string_copy(method_str, buffer, space_pos + 1);

    if (string_compare(method_str, "GET") == 0) return GET;
    if (string_compare(method_str, "POST") == 0) return POST;
    if (string_compare(method_str, "PUT") == 0) return PUT;
    if (string_compare(method_str, "DELETE") == 0) return DELETE;

    return UNKNOWN;
}

void parse_uri(const char* buffer, char* uri_out)
{
    const int first_space = find_str_in_str(buffer, " ", 0, 1);

    // if there is no space (first and second), it's a malform request
    if (first_space == -1)
    {
        uri_out[0] = '\0';
        return;
    }

    const int second_space = find_str_in_str(buffer, " ", first_space + 1, 1);

    if (second_space == -1)
    {
        uri_out[0] = '\0';
        return;
    }

    const size_t path_len = second_space - first_space - 1;

    // Limit to URI_MAX_LENGTH so an overly long path can't overflow the
    // caller's fixed-size uri buffer; string_copy will truncate at max_len - 1.
    const size_t copy_len = path_len + 1 < URI_MAX_LENGTH ? path_len + 1 : URI_MAX_LENGTH;

    string_copy(uri_out, buffer + first_space + 1, copy_len);
}

enum CONTENT_TYPE parse_content_type(const char* buffer)
{
    const int label_pos = find_str_in_str(buffer, HEADER_CONTENT_TYPE, 0, 1);

    if (label_pos == -1)
    {
        return CONTENT_TYPE_NONE;
    }

    const size_t value_start = label_pos + HEADER_CONTENT_TYPE_LEN;
    const int line_end = find_str_in_str(buffer + value_start, "\r\n", 0, 1);
    if (line_end == -1) return CONTENT_TYPE_NONE;

    char content_type_str[64];
    string_copy(content_type_str, buffer + value_start, line_end + 1);

    if (string_compare(content_type_str, "text/html") == 0) return TEXT_HTML;
    if (string_compare(content_type_str, "application/json") == 0) return APPLICATION_JSON;
    if (string_compare(content_type_str, "application/xml") == 0) return APPLICATION_XML;
    if (string_compare(content_type_str, "image/jpeg") == 0) return IMAGE_JPEG;
    if (string_compare(content_type_str, "image/png") == 0) return IMAGE_PNG;

    return CONTENT_TYPE_NONE;
}

size_t parse_content_length(const char* buffer)
{
    const int label_pos = find_str_in_str(buffer, HEADER_CONTENT_LENGTH, 0, 1);

    // If we do not see the header, this means there is no body
    if (label_pos == -1)
    {
        return 0;
    }

    const size_t value_start = label_pos + HEADER_CONTENT_LENGTH_LEN;

    size_t result = 0;
    size_t i = 0;
    while (buffer[value_start + i] >= '0' && buffer[value_start + i] <= '9')
    {
        // Do - '0' (48), so that the number will be given
        // For example 8 is 56 and 0 is 48, 56 - 48 = 8
        result = result * 10 + (buffer[value_start + i] - '0');
        i++;
    }

    return result;
}

enum HTTP_VERSION parse_http_version(const char* buffer)
{
    const int first_space = find_str_in_str(buffer, " ", 0, 1);
    const int second_space = find_str_in_str(buffer, " ", first_space + 1, 1);
    if (first_space == -1 || second_space == -1) return HTTP_VERSION_UNKNOWN;

    const int line_end = find_str_in_str(buffer + second_space + 1, "\r\n", 0, 1);
    if (line_end == -1) return HTTP_VERSION_UNKNOWN;

    char version_str[16];
    string_copy(version_str, buffer + second_space + 1, line_end + 1);

    if (string_compare(version_str, "HTTP/1.1") == 0) return HTTP_1_1;
    if (string_compare(version_str, "HTTP/1.0") == 0) return HTTP_1_0;

    return HTTP_VERSION_UNKNOWN;
}

bool parse_keep_alive(const char* buffer)
{
    const enum HTTP_VERSION version = parse_http_version(buffer);
    const int label_pos = find_str_in_str(buffer, HEADER_CONNECTION, 0, 1);

    if (label_pos == -1)
    {
        // if http 1.1, we default to true for the keep alive
        // otherwise false
        return version == HTTP_1_1;
    }

    const size_t value_start = label_pos + HEADER_CONNECTION_LEN;
    const int line_end = find_str_in_str(buffer + value_start, "\r\n", 0, 1);
    if (line_end == -1) return version == HTTP_1_1;

    char connection_value[32];
    string_copy(connection_value, buffer + value_start, line_end + 1);

    if (string_compare(connection_value, "close") == 0) return false;
    if (string_compare(connection_value, "keep-alive") == 0) return true;

    return version == HTTP_1_1;
}

void route_cleanup()
{
    free(routes);
    route_capacity = 0;
    route_count = 0;
    routes = nullptr;
}