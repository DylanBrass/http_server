//
// Created by dylanbrass on 2026-09-06.
//

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "httpserver.h"
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "utils/buffer.h"
#include "utils/queue.h"

volatile sig_atomic_t should_shutdown = 0;

void handle_shutdown_signal(const int _)
{
    (void)_;
    should_shutdown = true;
}

void close_server(const int socket_descriptor)
{
    close(socket_descriptor);
    route_cleanup();
    printf("Server shutting down\n");
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
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 431: return "Request Header Fields Too Large";
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
    case APPLICATION_JSON_UTF8: return "application/json; charset=utf-8";
    case CONTENT_TYPE_NONE: return nullptr;
    }

    return nullptr;
}

void write_http_header(const int current_connection, const Response response, const bool keep_alive)
{
    char response_headers[RESPONSE_HEADER_SIZE] = {0};
    const char* content_type = get_content_type_text(response.content_type);
    char content_type_line[96] = {0};

    if (content_type != nullptr)
    {
        snprintf(content_type_line, sizeof(content_type_line), "Content-Type: %s\r\n", content_type);
    }

    snprintf(response_headers, sizeof(response_headers),
             "HTTP/1.1 %d %s\r\n"
             "%s"
             "Content-Length: %zu\r\n"
             "Connection: %s\r\n"
             "\r\n",
             response.status_code,
             get_status_text(response.status_code),
             content_type_line,
             response.body_length,
             keep_alive ? "keep-alive" : "close"
    );

    write(current_connection, response_headers, get_length(response_headers));
}

void write_error_response(const int current_connection, const int status_code, const char* body)
{
    const Response response = {
        .status_code = status_code, .content_type = TEXT_HTML, .body = body,
        .body_length = get_length(body)
    };
    write_http_header(current_connection, response, false);
    write(current_connection, response.body, response.body_length);
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

int add_request_body(Buffer* buffer, const int current_connection, size_t bytes_already_read, Request* request)
{
    const StrSearchResult headers_end_search_result = find_str_in_str(buffer->data, HTTP_DELIMITER, 0, 1);

    char* body;

    if (!headers_end_search_result.found)
    {
        body = "";
        request->body_length = 0;
    }
    else
    {
        const size_t body_start = headers_end_search_result.position + HTTP_DELIMITER_LEN;
        const ContentLengthResult content_length_result = parse_content_length(buffer->data);

        if (!content_length_result.is_valid)
        {
            write_error_response(current_connection, 400, "<h1>400 Bad Request</h1>");
            return -1;
        }

        if (body_start > SIZE_MAX - content_length_result.value)
        {
            write_error_response(current_connection, 413, "<h1>413 Oversized request</h1>");
            return -1;
        }

        const size_t target_total = body_start + content_length_result.value;

        if (target_total > MAX_REQUEST_SIZE)
        {
            write_error_response(current_connection, 413, "<h1>413 Oversized request</h1>");
            return -1;
        }

        if (allocate_buffer(buffer, target_total + 1) < 0)
        {
            perror("buffer allocation failed");
            return -1;
        }

        bool connection_ended_early = false;

        while (bytes_already_read < target_total)
        {
            // This will read up to the size of the body declared in the header
            const size_t remaining_needed = target_total - bytes_already_read;
            const ssize_t bytes_read = read(current_connection, buffer->data + bytes_already_read,
                                            remaining_needed);

            if (bytes_read <= 0)
            {
                connection_ended_early = true;
                break;
            }

            bytes_already_read += (size_t)bytes_read;
            buffer->data[bytes_already_read] = '\0';
        }

        // the body pointer is set at the buffer pointer + where the body starts in the request
        body = buffer->data + body_start;

        // set the body length :
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
    return 0;
}

int handle_client_connection(const int current_connection, Buffer* buffer)
{
    int requests_served = 0;

    while (true)
    {
        size_t total_bytes_read = 0;

        while (true)
        {
            if (total_bytes_read >= MAX_HEADER_SIZE)
            {
                write_error_response(current_connection, 431, "<h1>431 Request Header Fields Too Large</h1>");
                free_buffer(buffer);
                close(current_connection);
                return 0;
            }

            if (allocate_buffer(buffer, total_bytes_read + GROWTH_CHUNK) < 0)
            {
                perror("buffer allocation failed");
                free_buffer(buffer);
                close(current_connection);
                return 0;
            }

            const ssize_t bytes_read = read(current_connection, buffer->data + total_bytes_read,
                                            buffer->capacity - 1 - total_bytes_read);

            if (bytes_read <= 0)
            {
                free_buffer(buffer);
                close(current_connection);
                return 0;
            }

            total_bytes_read += (size_t)bytes_read;
            buffer->data[total_bytes_read] = '\0';

            const StrSearchResult headers_end = find_str_in_str(buffer->data, HTTP_DELIMITER, 0, 1);

            if (headers_end.found)
            {
                if (headers_end.position + HTTP_DELIMITER_LEN > MAX_HEADER_SIZE)
                {
                    write_error_response(current_connection, 431, "<h1>431 Request Header Fields Too Large</h1>");
                    free_buffer(buffer);
                    close(current_connection);
                    return 0;
                }
                break;
            }
        }

        if (uri_too_long(buffer->data))
        {
            write_error_response(current_connection, 414, "<h1>414 URI Too Long</h1>");
            free_buffer(buffer);
            close(current_connection);
            return 0;
        }

        char uri[URI_MAX_LENGTH];
        parse_uri(buffer->data, uri);

        const enum HTTP_METHOD http_method = parse_http_method(buffer->data);
        const RouteHandler handler = find_route(http_method, uri);

        Request request;
        const int result = add_request_body(buffer, current_connection, total_bytes_read, &request);

        if (result < 0)
        {
            free_buffer(buffer);
            close(current_connection);
            return -1;
        }

        request.method = http_method;
        string_copy(request.uri, uri, URI_MAX_LENGTH);
        request.content_type = parse_content_type(buffer->data);

        requests_served++;
        // Limit the amount of requests a specific connection can do after being kept alive
        // this is to avoid a specific client from holding a thread forever and
        // allow other connections to be handled.
        const bool keep_alive = parse_keep_alive(buffer->data)
            && requests_served < MAX_KEEPALIVE_REQUESTS;

        const Response response = create_response(handler, &request);

        write_http_header(current_connection, response, keep_alive);
        write(current_connection, response.body, response.body_length);

        if (!keep_alive)
        {
            free_buffer(buffer);
            close(current_connection);
            return 0;
        }
    }
}

void* handle_incoming_thread(void* arg)
{
    ConnectionQueue* queue = arg;

    while (true)
    {
        const int current_connection = pop_connection(queue);

        if (current_connection < 0)
        {
            break;
        }

        // Creates a timeout of 5 seconds and 0 microseconds aka 5.0
        const struct timeval timeout = {.tv_sec = KEEP_ALIVE_TIMEOUT_SECONDS, .tv_usec = 0};
        // set the sock option to timeout after the timeout set above.
        // SO_RCVTIMEO is the option to modify to set the timeout.
        // This is so a worker does not get stuck forever if it crashes
        // or the connection stop sending requests.
        if (setsockopt(current_connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            perror("setsockopt SO_RCVTIMEO failed");
        }

        Buffer buffer = {0};

        if (allocate_buffer(&buffer, INIT_BUFFER_SIZE) < 0)
        {
            perror("initial buffer allocation failed");
            close(current_connection);
            continue;
        }

        if (handle_client_connection(current_connection, &buffer) < 0)
        {
            printf("Failed to handle client connection\n");
        }
    }

    return nullptr;
}

int run_server(const int socket_descriptor)
{
    ConnectionQueue queue;

    if (queue_init(&queue) < 0)
    {
        perror("queue init failed");
        return -1;
    }

    pthread_t workers[THREAD_POOL_SIZE];
    int workers_started = 0;

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        const int create_result = pthread_create(&workers[i], nullptr, handle_incoming_thread, &queue);

        if (create_result != 0)
        {
            fprintf(stderr, "pthread_create failed for worker %d: %s\n", i, strerror(create_result));
            break;
        }

        workers_started++;
    }

    if (workers_started == 0)
    {
        fprintf(stderr, "No worker threads could be started\n");
        queue_destroy(&queue);
        return -1;
    }

    while (true)
    {
        const int current_connection = accept(socket_descriptor, nullptr, nullptr);

        if (current_connection < 0)
        {
            if (should_shutdown) break;

            if (errno == EMFILE || errno == ENFILE)
            {
                const struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000 * 1000};
                nanosleep(&pause, nullptr);
                continue;
            }

            if (errno == EINTR)
            {
                continue;
            }

            perror("accept failed");
            continue;
        }

        push_connection(&queue, current_connection);
    }

    queue_shutdown(&queue);

    for (int i = 0; i < workers_started; i++)
    {
        pthread_join(workers[i], nullptr);
    }

    if (queue_destroy(&queue) < 0)
    {
        fprintf(stderr, "Failed to destroy the queue\n");
    }

    return 0;
}

int start_server(const uint16_t port)
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
    // Some CPUs will store 1F 90 since they put the biggest bytes first and others
    // will store 90 1F (smallest bytes first). htons is host to network, meaning it converts the number 8080 (90 1F)
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
        close(socket_descriptor);
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
        close(socket_descriptor);
        return -1;
    }

    const int listen_result = listen(socket_descriptor, MAX_CONNECTIONS);

    if (listen_result < 0)
    {
        perror("listen failed");
        close(socket_descriptor);
        return -1;
    }

    printf("Server listening on port %d\n", port);
    // Handle the pipe broken errors
    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    sa.sa_handler = handle_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    const int run_result = run_server(socket_descriptor);
    close_server(socket_descriptor);

    return run_result;
}
