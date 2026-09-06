#include <stdio.h>

#include "httpserver.h"

Response handle_status(const Request* _)
{
    static char json_buffer[256];
    snprintf(json_buffer, sizeof(json_buffer),
        "{\"status\":\"ok\",\"uptime\":%d}", 42);

    return (Response){
        .status_code = 200,
        .content_type = APPLICATION_JSON,
        .body = json_buffer,
        .body_length = get_length(json_buffer)
    };
}

Response handle_static_page(const Request* request)
{
    static char file_buffer[4096];

    FILE* fp = fopen("../www/test.html", "r");

    if (fp == nullptr)
    {
        const char* error_body = "<h1>500 Internal Server Error</h1>";
        return (Response){
            .status_code = 500,
            .content_type = TEXT_HTML,
            .body = error_body,
            .body_length = get_length(error_body)
        };
    }

    const size_t bytes_read = fread(file_buffer, 1, sizeof(file_buffer) - 1, fp);
    file_buffer[bytes_read] = '\0';
    fclose(fp);

    return (Response){
        .status_code = 200,
        .content_type = TEXT_HTML,
        .body = file_buffer,
        .body_length = bytes_read
    };
}

Response handle_post(const Request* request)
{
    printf("POST body: %.*s\n", (int)request->body_length, request->body);

    const char* body = request->body;
    return (Response){
        .status_code = 201, .content_type = APPLICATION_JSON, .body = body, .body_length = request->body_length
    };
}

Response handle_update(const Request* request)
{
    printf("PUT body: %.*s\n", (int)request->body_length, request->body);

    const char* body = request->body;
    return (Response){
        .status_code = 200,
        .content_type = APPLICATION_JSON,
        .body = body,
        .body_length = get_length(body)
    };
}

Response handle_delete(const Request* request)
{
    printf("DELETE %s\n", request->uri);

    const char* body = "";
    return (Response){
        .status_code = 204,
        .content_type = APPLICATION_JSON,
        .body = body,
        .body_length = get_length(body)
    };
}


int main(void)
{
    register_route(GET, CONTENT_TYPE_NONE, "/test", handle_status);
    register_route(GET, CONTENT_TYPE_NONE, "/test/html", handle_static_page);
    register_route(POST, APPLICATION_JSON, "/test", handle_post);
    register_route(PUT, APPLICATION_JSON, "/test", handle_update);
    register_route(DELETE, CONTENT_TYPE_NONE, "/test", handle_delete);
    start_server(8080);
    return 0;
}
