//
// Created by dylanbrass on 2026-09-06.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "httpserver.h"
#include <signal.h>

volatile sig_atomic_t should_shutdown = 0;

void handle_shutdown_signal(int _)
{
    should_shutdown = true;
}

const char* get_status_text(const int status_code)
{
    switch (status_code)
    {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default: return "Unknown";
    }
}

const char* get_content_type_text(const enum CONTENT_TYPE content_type)
{
    switch (content_type)
    {
    case TEXT_HTML: return "text/html";
    case APPLICATION_JSON: return "application/json";
    case APPLICATION_XML: return "application/xml";
    case IMAGE_JPEG: return "image/jpeg";
    case IMAGE_PNG: return "image/png";
    default: return "Unknown";
    }
}

void write_http_header(const int current_connection, const Response response)
{
    char response_headers[RESPONSE_HEADER_SIZE] = {0};
    snprintf(
        response_headers,
        sizeof(response_headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        response.status_code,
        get_status_text(response.status_code),
        get_content_type_text(response.content_type),
        response.body_length
    );

    write(current_connection, response_headers, get_length(response_headers));
}

Response create_response(const RouteHandler handler, const Request* request)
{
    Response response;

    if (handler == nullptr)
    {
        const char* not_found_body = "<h1>404 Not Found</h1>";
        response = (Response){
            .status_code = 404, .content_type = TEXT_HTML, .body = not_found_body,
            .body_length = get_length(not_found_body)
        };
    }
    else
    {
        response = handler(request);
    }
    return response;
}

void add_request_body(char* buffer, const int current_connection, size_t bytes_already_read, Request* request)
{
    const int headers_end = find_str_in_str(buffer, HTTP_DELIMITER, 0, 1);

    char* body;

    if (headers_end == -1)
    {
        body = "";
        request->body_length = 0;
    }
    else
    {
        const size_t body_start = headers_end + HTTP_DELIMITER_LEN;
        const size_t content_length = parse_content_length(buffer);
        size_t target_total = body_start + content_length;

        if (target_total > BUFFER_SIZE - 1)
        {
            target_total = BUFFER_SIZE - 1;
        }

        bool connection_ended_early = false;

        while (bytes_already_read < target_total && bytes_already_read < BUFFER_SIZE - 1)
        {
            const ssize_t bytes_read = read(current_connection, buffer + bytes_already_read,
                                            BUFFER_SIZE - 1 - bytes_already_read);

            if (bytes_read <= 0)
            {
                connection_ended_early = true;
                break;
            }

            bytes_already_read += bytes_read;
            buffer[bytes_already_read] = '\0';
        }

        // the body pointer is set at the buffer pointer + where the body starts in the request
        body = buffer + body_start;

        // set the body length to :
        // if the connection stopped as expected, simply return how many bytes
        // the body is the total claimed (target_total) - the start of the body
        // if it ended early and we read more than where the body starts:
        // then we return the total size of the read body right up to the disconnect
        // else we return 0, since we would not have even started to read the body.
        request->body_length = connection_ended_early
                                   ? (bytes_already_read > body_start ? bytes_already_read - body_start : 0)
                                   : target_total - body_start;
    }


    request->body = body;
}

void handle_client_connection(const int current_connection, char buffer[BUFFER_SIZE])
{
    // printf("Client connected\n");

    ssize_t total_bytes_read = 0;

    while (total_bytes_read < BUFFER_SIZE - 1)
    {
        const ssize_t bytes_read = read(current_connection, buffer + total_bytes_read,
                                        BUFFER_SIZE - 1 - total_bytes_read);

        if (bytes_read < 0)
        {
            perror("read failed");
            break;
        }

        if (bytes_read == 0)
        {
            // printf("Client disconnected\n");
            break;
        }

        total_bytes_read += bytes_read;
        buffer[total_bytes_read] = '\0';

        if (find_str_in_str(buffer, HTTP_DELIMITER, 0, 1) != -1)
        {
            break;
        }
    }

    char uri[URI_MAX_LENGTH];
    parse_uri(buffer, uri);

    const enum HTTP_METHOD http_method = parse_http_method(buffer);

    const RouteHandler handler = find_route(http_method, uri);

    // printf("method=%d uri=%s\n", http_method, uri);


    Request request;
    add_request_body(buffer, current_connection, total_bytes_read, &request);
    request.method = http_method;
    string_copy(request.uri, uri, URI_MAX_LENGTH);
    request.content_type = parse_content_type(buffer);

    const Response response = create_response(handler, &request);

    write_http_header(current_connection, response);
    write(current_connection, response.body, response.body_length);

    close(current_connection);
    // printf("Client disconnected\n");
}

void run_server(const int socket_descriptor)
{
    // TODO: make it multi threaded
    while (true)
    {
        const int current_connection = accept(socket_descriptor, nullptr, nullptr);
        char buffer[BUFFER_SIZE] = {0};


        if (current_connection < 0)
        {
            if (should_shutdown)
            {
                break;
            }

            perror("accept failed");
            continue;
        }


        handle_client_connection(current_connection, buffer);
    }
}

int start_server(const int port)
{
    // AF_INET = IPV4, SOCK_STREAM = TCP, and 0 = default which is TCP in this case
    const int socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_descriptor < 0)
    {
        perror("socket failed to be established");
        return -1;
    }

    struct sockaddr_in address = {0};

    address.sin_family = AF_INET;
    // htons transforms the number 8080 in this case
    // which is 1F90 in hexa to the correct form depending on the CPU architecture.
    // Some CPUs will store 90 1F since they put the biggest bytes first and others
    // will store 1F 90 (smallest bytes first). htons is host to network, meaning it converts the number 8080 (90 1F)
    // to what the network expects which is Big-endian, biggest bytes first.
    // Basically big-endian is putting the biggest bytes in the first memory addr
    // and small-endian is putting the smallest bytes in the lowest memory addr
    // In a grid of horizontal boxes, lowest would be leftmost and highest rightmost.
    // This is assuming your memory increases the more you got right, which is what is most common
    // in text books.
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    constexpr int optval = 1;
    // setsockopt is to set options for sockets, in this case the one we are creating
    // SOL_SOCKET is targeting the socket itself rather than a protocol
    // with SO_REUSEADDR we are saying that the socket can be bound to another socket to the port
    // in specific conditions and avoid port is unavailable errors.
    // In our case the most important one is that this socket can be replaced when it
    // enters TIME_WAIT.
    const int setsockopt_result = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (setsockopt_result < 0)
    {
        perror("setsockopt failed");
        return -1;
    }

    // Ask the OS to reserve the port for the file descriptor.
    // A socket descriptor is a number that represents a process resource, in our case,
    // we are creating this socket descriptor when creating the socket,
    // all future operations are then done on by telling the kernel what resource we are targeting
    // with this descriptor.
    const int bind_result = bind(socket_descriptor, (struct sockaddr*)&address, sizeof(address));

    if (bind_result < 0)
    {
        perror("bind failed");
        return -1;
    }

    // 10 is the max number of pending (not yet accepted) connections that can be in the queue
    const int listen_result = listen(socket_descriptor, 10);

    if (listen_result < 0)
    {
        perror("listen failed");
        return -1;
    }

    printf("Server listening on port 8080\n");

    struct sigaction sa;
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    run_server(socket_descriptor);

    close(socket_descriptor);
    printf("Server shutting down\n");

    return 0;
}
