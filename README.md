# http_server

A minimal HTTP/1.1 server written from scratch in C — no external HTTP, socket, or string libraries. Built as a learning project to explore raw TCP sockets, manual HTTP parsing, and library/API design in C.

Everything is implemented on top of POSIX sockets and the standard C library only: request parsing, routing, and even core string utilities (`strlen`, `strcmp`, `strcpy`, `strstr` equivalents) are hand-written rather than pulled from `<string.h>`.

## Features

- Raw TCP socket server (`socket`/`bind`/`listen`/`accept`) with `SO_REUSEADDR`
- Hand-written HTTP/1.1 request parsing: method, URI, headers, and body
- Support for `GET`, `POST`, `PUT`, and `DELETE`
- Request bodies read according to `Content-Length`, exposed to handlers
- `Content-Type` parsing for incoming requests
- A small routing API: register a handler per (method, path) pair, backed by a route table that grows dynamically (`realloc`) instead of a fixed-size array
- Per-connection read buffer that grows dynamically (`realloc`) as data arrives, instead of a fixed-size buffer
- Structured `Request` / `Response` types passed to and returned from handlers
- HTTP/1.1 keep-alive: connections are reused across requests based on the `Connection` header (defaulting to keep-alive on HTTP/1.1, close on HTTP/1.0), capped per connection by `MAX_KEEPALIVE_REQUESTS` so a single client can't monopolize a worker thread
- Per-connection idle timeout (`SO_RCVTIMEO`, `KEEP_ALIVE_TIMEOUT_SECONDS`) so a worker isn't stuck forever on a stalled or abandoned connection
- Request size limits: oversized headers return `400 Bad Request` (`MAX_HEADER_SIZE`) and oversized bodies return `413 Content Too Large` (`MAX_REQUEST_SIZE`)
- Fixed-size worker thread pool (`THREAD_POOL_SIZE`) that handles connections concurrently, backed by a bounded, synchronized connection queue (mutex + condition variables) — the accept loop only ever pushes fds; workers pop and handle them
- Graceful shutdown on `SIGINT`/`SIGTERM` via `sigaction`, including a coordinated queue shutdown (broadcast + `pthread_join` on every worker) before cleanup
- Built as a static library (`httpserver`) consumed by a separate example executable (`dashboard_server`)

## Project structure

```
http_server/
├── include/
│   └── httpserver.h       # Public API: types, enums, function declarations
├── src/
│   ├── server.c            # Socket lifecycle, request/response I/O, shutdown handling
│   ├── router.c            # Route registration/matching, request-line & header parsing
│   └── utils/
│       ├── buffer.c        # Dynamic (realloc-based) growable buffer used for connection reads
│       ├── buffer.h        # Buffer type and API
│       ├── queue.c         # Thread-safe bounded connection queue (mutex + condition variables)
│       ├── queue.h         # ConnectionQueue type and API
│       └── str_functions.c # Hand-written string utilities (length, compare, copy, search)
├── example/
│   └── main.c              # Example consumer: registers routes, starts the server
├── www/
│   └── test.html            # Sample static file served by the example
└── CMakeLists.txt
```

## Building

Requires CMake and a C23-capable compiler (GCC 13+/Clang 17+ recommended).

```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

This produces the `httpserver` static library and the `dashboard_server` example executable.

## Running the example

```bash
./dashboard_server
```

The server listens on port `8080`. Example routes registered in `example/main.c`:

| Method | Path         | Description                         |
|--------|--------------|-------------------------------------|
| GET    | `/test`      | Returns a small JSON status payload |
| GET    | `/test/html` | Serves `www/test.html` from disk    |
| POST   | `/test`      | Echoes the JSON body back           |
| PUT    | `/test`      | Echoes the JSON body back           |
| DELETE | `/test`      | Returns `204 No Content`            |

Try it with `curl`:

```bash
curl http://localhost:8080/test
curl http://localhost:8080/test/html
curl -X POST -H "Content-Type: application/json" -d '{"x":1}' http://localhost:8080/test
```

Stop the server with `Ctrl+C` — it shuts down gracefully rather than being killed outright.

## Using the library in your own code

```c
#include "httpserver.h"

Response handle_home(const Request* request)
{
    const char* body = "<html><body><h1>Hello</h1></body></html>";
    return (Response){
        .status_code = 200,
        .content_type = TEXT_HTML,
        .body = body,
        .body_length = get_length(body)
    };
}

int main(void)
{
    register_route(GET, CONTENT_TYPE_NONE, "/", handle_home);
    start_server(8080);
    return 0;
}
```

## Possible/Known limitations

- No HTTPS/TLS support
- No support for `Transfer-Encoding: chunked` bodies
- No IPv6 support (the listening socket is `AF_INET`/`sockaddr_in` only)
- Only `GET`, `POST`, `PUT`, and `DELETE` are recognized; there's no `HEAD`, `OPTIONS`, `PATCH`, etc.
- Routing is exact-match only — no path parameters (`/users/:id`) or wildcards
- A route's declared `request_body_content_type` is stored at registration but never validated against the incoming request's actual `Content-Type`
- No URL decoding or query-string parsing — the request path is matched literally, so `%20`-style escapes and `?key=value` query strings aren't handled
- A fixed pool of `THREAD_POOL_SIZE` (1024) OS threads is spawned unconditionally at startup rather than using an event-driven (e.g. epoll) model, which is comparatively heavy on memory/OS resources and isn't adjustable at runtime
- `register_route` is not thread-safe; all routes must be registered before `start_server()` begins accepting connections
- No access/request logging
- A few header-value fields (HTTP method, `Content-Type`) are parsed into small fixed-size stack buffers sized for well-formed input; malformed or oversized values aren't yet bounds-checked against these buffers, so this needs hardening before being exposed to untrusted clients
- `parse_content_length` accumulates digits into a `size_t` with no overflow check or digit-count cap; an oversized `Content-Length` value can wrap around, and a resulting `target_total` smaller than `body_start` would underflow the `size_t` subtraction in `add_request_body`, producing a bogus (potentially huge) `body_length`
- `write()` calls (in `write_http_header`, `write_error_response`, and the response body writes in `handle_client_connection`) aren't checked for partial writes; `write()` may send fewer bytes than requested, which today would silently truncate a response and desync a kept-alive connection
- Only an idle timeout is enforced (`SO_RCVTIMEO`), and it resets on every successful `read()` no matter how few bytes were read; a client that trickles in a byte or two just before each timeout expires can hold a worker thread indefinitely without ever completing a request (a Slowloris-style pattern) — there's no overall wall-clock deadline for finishing a request's headers or body
- No validation that HTTP/1.1 requests include a `Host` header, which the spec requires
- Header lookups (`Content-Type`, `Content-Length`, `Connection`) search for the header name as a raw substring anywhere in the buffer rather than anchoring to the start of a line; a request containing an unrelated header whose name ends in one of these (e.g. `X-Original-Content-Type:`) can be misread as the real header
- Header name and value matching is case-sensitive, while HTTP header names and common token values (e.g. `Content-Type`, `keep-alive`) are case-insensitive per spec; differently-cased input from a client is not recognized
- `Content-Type` values with parameters (e.g. `application/json; charset=utf-8`) don't match any of the known exact strings and fall back to `CONTENT_TYPE_NONE`, even though this is a very common form in real requests
- Header parsing assumes exactly one space after the colon (e.g. `"Content-Length: "`); a spec-legal header with no space or extra spaces after the colon either isn't found or breaks value parsing
- Only `\r\n\r\n` is recognized as the end-of-headers delimiter; a client sending bare `\n` line endings will stall until `MAX_HEADER_SIZE` is hit and get a `400`
- No HTTP pipelining support: if a single `read()` returns bytes belonging to a second, already-pipelined request past the end of the current one's body, those extra bytes are never carried forward — the next loop iteration discards the buffer and reads fresh from the socket, so a pipelining client can see corrupted or missing responses