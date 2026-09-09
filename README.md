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
- Graceful shutdown on `SIGINT`/`SIGTERM` via `sigaction`
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

## Known limitations

- Single-threaded — one connection is fully handled before the next is accepted
- No HTTPS/TLS support
- No support for `Transfer-Encoding: chunked` bodies
- A few header-value fields (HTTP method, `Content-Type`) are parsed into small fixed-size stack buffers sized for well-formed input; malformed or oversized values aren't yet bounds-checked against these buffers, so this needs hardening before being exposed to untrusted clients
- The request line parser (`parse_uri`) assumes a well-formed `METHOD /path HTTP/x.x` line; a request line missing its second space is not yet handled defensively