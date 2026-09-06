//
// Created by dylanbrass on 2026-09-06.
//
#include <stdio.h>

#include "httpserver.h"

static Route routes[MAX_ROUTES];
static int route_count = 0;

int register_route(const enum HTTP_METHOD http_method, const enum CONTENT_TYPE request_body_content_type,
                   const char* uri, const RouteHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        perror("route count exceeds MAX_ROUTES");
        return -1;
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
    const int space_pos = find_str_in_str(buffer, " ", 1);
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
    const int first_space = find_str_in_str(buffer, " ", 1);
    const int second_space = find_str_in_str(buffer, " ", 2);

    const size_t path_len = second_space - first_space - 1;

    string_copy(uri_out, buffer + first_space + 1, path_len + 1);
}

enum CONTENT_TYPE parse_content_type(const char* buffer)
{
    const int label_pos = find_str_in_str(buffer, "Content-Type: ", 1);

    if (label_pos == -1)
    {
        return CONTENT_TYPE_NONE;
    }

    const size_t value_start = label_pos + get_length("Content-Type: ");
    const int line_end = find_str_in_str(buffer + value_start, "\r\n", 1);

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
    const int label_pos = find_str_in_str(buffer, "Content-Length: ", 1);

    // If we do not see the header, this means there is no body
    if (label_pos == -1)
    {
        return 0;
    }

    const size_t value_start = label_pos + get_length("Content-Length: ");

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
