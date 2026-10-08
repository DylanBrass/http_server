//
// Created by dylanbrass on 2026-09-06.
//

#ifndef HTTP_SERVER_HTTPSERVER_H
#define HTTP_SERVER_HTTPSERVER_H

#include <stdint.h>
#include <stddef.h>

#define HTTP_DELIMITER "\r\n\r\n"
#define HTTP_DELIMITER_LEN (sizeof(HTTP_DELIMITER) - 1)

#define HEADER_CONTENT_TYPE "content-type"
#define HEADER_CONTENT_LENGTH "content-length"
#define HEADER_CONNECTION "connection"

#define KEEP_ALIVE_TIMEOUT_SECONDS 5
#define MAX_KEEPALIVE_REQUESTS 1000

#define RESPONSE_HEADER_SIZE 256

#define INIT_BUFFER_SIZE 1024

#define URI_MAX_LENGTH 256

#define MAX_REQUEST_SIZE 1048576
#define MAX_HEADER_SIZE 16384

#define MAX_CONNECTIONS 2048
#define THREAD_POOL_SIZE 1024

int start_server(uint16_t port);

size_t get_length(const char* arr);

int string_compare(const char* str1, const char* str2);

int string_copy(char* target, const char* source, size_t max_len);


typedef struct
{
    size_t position;
    bool found;
} StrSearchResult;

StrSearchResult find_str_in_str(const char* haystack, const char* needle, size_t start_from, size_t target_occurrence);

int string_to_lower(char *str);

char char_to_lower(char c);

enum CASE_SENSITIVITY
{
    CASE_SENSITIVE,
    CASE_INSENSITIVE,
};

typedef struct
{
    const char* data;
    size_t length;
} StrSpan;

StrSpan span_from_string(const char* str);

bool span_equals(StrSpan first, StrSpan second, enum CASE_SENSITIVITY case_sensitivity);

bool next_delimited(StrSpan* rest,char separator, StrSpan* item);

StrSpan span_trim(StrSpan span);

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
    APPLICATION_JSON_UTF8,
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

typedef struct
{
    size_t value;
    bool is_valid;
} ContentLengthResult;

typedef struct
{
    StrSpan method;
    StrSpan uri;
    StrSpan version;
} RequestLine;

typedef struct
{
    StrSpan value;
    size_t count;
    bool conflict;
} HeaderLookup;

typedef Response (*RouteHandler)(const Request* request);

typedef struct Route
{
    enum HTTP_METHOD http_method;
    enum CONTENT_TYPE request_body_content_type;
    char uri[URI_MAX_LENGTH];
    RouteHandler handler;
} Route;

enum HTTP_VERSION
{
    HTTP_1_0,
    HTTP_1_1,
    HTTP_VERSION_UNKNOWN,
};

int register_route(enum HTTP_METHOD http_method, enum CONTENT_TYPE request_body_content_type
                   , const char* uri, RouteHandler handler);

RouteHandler find_route(const enum HTTP_METHOD http_method, const char* uri);

void parse_uri(const char* buffer, char* uri_out);

bool uri_too_long(const char* buffer);

enum HTTP_METHOD parse_http_method(const char* buffer);

enum CONTENT_TYPE parse_content_type(const char* buffer);

ContentLengthResult parse_content_length(const char* buffer);

enum HTTP_VERSION parse_http_version(const char* buffer);

bool parse_keep_alive(const char* buffer);

void route_cleanup();

#endif //HTTP_SERVER_HTTPSERVER_H