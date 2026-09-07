//
// Created by dylanbrass on 2026-09-06.
//

#ifndef HTTP_SERVER_HTTPSERVER_H
#define HTTP_SERVER_HTTPSERVER_H

#define HTTP_DELIMITER "\r\n\r\n"
// - 1 to take into account \0 (the indication of the end of string
#define HTTP_DELIMITER_LEN (sizeof(HTTP_DELIMITER) - 1)

#define HEADER_CONTENT_TYPE "Content-Type: "
// - 1 to take into account \0 (the indication of the end of string
#define HEADER_CONTENT_TYPE_LEN (sizeof(HEADER_CONTENT_TYPE) - 1)

#define HEADER_CONTENT_LENGTH "Content-Length: "
// - 1 to take into account \0 (the indication of the end of string
#define HEADER_CONTENT_LENGTH_LEN (sizeof(HEADER_CONTENT_LENGTH) - 1)

#define RESPONSE_HEADER_SIZE 256
// TODO: make it dynamic
#define MAX_ROUTES 32
// TODO: make it dynamic for bigger requests
#define BUFFER_SIZE 1024

#define URI_MAX_LENGTH 256

int start_server(int port);

size_t get_length(const char* arr);

int string_compare(const char* str1, const char* str2);

int string_copy(char* target, const char* source, size_t max_len);

int find_str_in_str(const char* haystack, const char* needle, size_t start_from, size_t target_occurrence);

enum HTTP_METHOD
{
    GET,
    POST,
    PUT,
    DELETE,
    UNKNOWN,
};

enum CONTENT_TYPE
{
    TEXT_HTML,
    APPLICATION_XML,
    APPLICATION_JSON,
    IMAGE_JPEG,
    IMAGE_PNG,
    CONTENT_TYPE_NONE,
};

typedef struct
{
    int status_code;
    enum CONTENT_TYPE content_type;
    const char* body;
    size_t body_length;
} Response;

typedef struct
{
    enum HTTP_METHOD method;
    char uri[URI_MAX_LENGTH];
    enum CONTENT_TYPE content_type;
    const char* body;
    size_t body_length;
} Request;

typedef Response (*RouteHandler)(const Request* request);

typedef struct Route
{
    enum HTTP_METHOD http_method;
    enum CONTENT_TYPE request_body_content_type;
    char uri[URI_MAX_LENGTH];
    RouteHandler handler;
} Route;

int register_route(enum HTTP_METHOD http_method, enum CONTENT_TYPE request_body_content_type
                   , const char* uri, RouteHandler handler);

RouteHandler find_route(enum HTTP_METHOD http_method, char* uri);

void parse_uri(const char* buffer, char* uri_out);

enum HTTP_METHOD parse_http_method(const char* buffer);

enum CONTENT_TYPE parse_content_type(const char* buffer);

size_t parse_content_length(const char* buffer);

#endif //HTTP_SERVER_HTTPSERVER_H
