# http_server

A small HTTP/1.1 server library written from scratch in C23 as a learning project. It provides exact-match routing,
keep-alive connections, request body parsing with size limits, and a fixed thread pool. It is built as a static
library (`httpserver`) plus an example executable (`dashboard_server`).

## Building

Requires CMake 4.1+ and a C23-capable compiler (GCC 13+ or Clang 17+ recommended; the code uses `nullptr`, `constexpr`,
and `thread_local` as keywords).

```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

This produces the `httpserver` static library and the `dashboard_server` example executable.

**Strict warnings and sanitizers are on by default.** `CMakeLists.txt` compiles with
`-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror` and links with `-fsanitize=address,undefined`. Two
consequences:

- Any warning fails the build, including unused parameters and implicit signed/unsigned conversions.
- The ASan/UBSan runtimes must be installed, or linking fails with `cannot find libasan.so` / `libubsan.so`. On
  Fedora/RHEL: `sudo dnf install libasan libubsan`. On Debian/Ubuntu: `sudo apt install libasan8 libubsan1` (the version
  must match your GCC).

To build without sanitizers, remove the `-fsanitize=...` lines from `add_compile_options` and `add_link_options` in
`CMakeLists.txt`.

## Running the example

Run it from inside `cmake-build-debug` (the static page route opens `../www/test.html` relative to the working
directory):

```bash
./dashboard_server
```

The server listens on port `8080`. Routes registered in `example/main.c`:

| Method | Path         | Status | Description                                                         |
|--------|--------------|--------|---------------------------------------------------------------------|
| GET    | `/test`      | 200    | Returns a small JSON status payload (`uptime` is a hard-coded `42`) |
| GET    | `/test/html` | 200    | Serves `www/test.html` from disk (truncated at 4095 bytes)          |
| POST   | `/test`      | 201    | Echoes the request body back                                        |
| PUT    | `/test`      | 200    | Echoes the request body back                                        |
| DELETE | `/test`      | 204    | Returns `204 No Content`                                            |

Try it with `curl`:

```bash
curl http://localhost:8080/test
curl http://localhost:8080/test/html
curl -X POST -H "Content-Type: application/json" -d '{"x":1}' http://localhost:8080/test
```

Stop the server with `Ctrl+C`. It shuts down gracefully rather than being killed outright (see "Shutdown behavior"
below).

## Using the library in your own code

```c
#include <stdio.h>

#include "httpserver.h"

Response handle_home(const Request* request)
{
    (void)request; // -Werror: unused parameters are errors

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
    if (register_route(GET, CONTENT_TYPE_NONE, "/", handle_home) < 0)
    {
        fprintf(stderr, "Failed to register route\n");
        return 1;
    }

    return start_server(8080) == 0 ? 0 : 1;
}
```

### Writing handlers

A handler has the signature `Response (*)(const Request*)`. The `Request` contains `method`, `uri`, `content_type`,
`body`, and `body_length`. Things to keep in mind:

- **Response body lifetime.** The server writes the response *after* your handler returns and never frees the body, so
  the body must still be valid at that point. Use string literals, `static`/`thread_local` buffers (as the example
  does), or point at `request->body` to echo it back. A `malloc`'d body would leak, since there is no cleanup hook.
- **Handlers run concurrently** on the worker threads, so they must be thread-safe. That is why the example uses
  `thread_local` buffers rather than plain `static` ones.
- **Use `body_length`, not `get_length(body)`,** for request bodies. Bodies are not guaranteed to be text and may
  contain `NUL` bytes.
- **No custom response headers.** `Response` has no header field, so handlers can't set `Location`, `Set-Cookie`, etc. (
  a `301`/`302` can't carry a `Location`).
- **Use a real `content_type`.** Returning `CONTENT_TYPE_NONE` produces an invalid `Content-Type: Unknown` header.
- **Status codes with reason phrases:** 200, 201, 204, 301, 302, 400, 401, 403, 404, 405, 413, 500, 501. Any other code
  is sent with the reason phrase `Unknown`.
- **Register routes before calling `start_server()`.** `register_route` returns `-1` if memory allocation fails.

## Configuration

These are compile-time macros in `include/httpserver.h`:

| Macro                        | Default | Meaning                                                        |
|------------------------------|---------|----------------------------------------------------------------|
| `THREAD_POOL_SIZE`           | 1024    | Worker threads spawned at startup                              |
| `MAX_CONNECTIONS`            | 2048    | Pending-connection queue size and `listen()` backlog           |
| `KEEP_ALIVE_TIMEOUT_SECONDS` | 5       | Idle timeout (`SO_RCVTIMEO`) on each connection                |
| `MAX_KEEPALIVE_REQUESTS`     | 1000    | Requests served on one connection before it is closed          |
| `MAX_HEADER_SIZE`            | 16384   | Approximate cap on request headers; exceeding it returns `400` |
| `MAX_REQUEST_SIZE`           | 1048576 | Cap on headers + body; exceeding it returns `413`              |
| `URI_MAX_LENGTH`             | 256     | Request URI buffer size (longer URIs are truncated)            |

### Error responses

- Unmatched route (any method or path, including an unrecognized method): `404`.
- Invalid `Content-Length` (present but non-numeric/empty): `400`.
- Headers too large: `400`.
- Declared body too large (or a length that overflows): `413`.

Connections are closed after any of these errors.

## Concurrency model and its limits

This server uses **thread-per-connection**: each accepted connection is handed to one worker thread from a fixed-size
pool (`THREAD_POOL_SIZE`), and that thread stays dedicated to that connection for as long as it stays open.

With HTTP keep-alive enabled, a connection can stay open for many requests in a row, which means **`THREAD_POOL_SIZE` is
a hard ceiling on the number of concurrent persistent connections the server can actively serve**, not just a throughput
knob. If more clients hold open keep-alive connections than there are worker threads, the excess connections queue (up
to `MAX_CONNECTIONS`) and wait for a worker to free up, which only happens when some other client's connection closes,
sends its `MAX_KEEPALIVE_REQUESTS`th request, or goes idle past the keep-alive timeout.

This is an intentional simplicity tradeoff for a from-scratch learning project, not a bug: thread-per-connection is
straightforward to reason about, but doesn't scale to large numbers of concurrent (mostly idle) persistent connections
the way an event-loop model (`epoll`/`kqueue`-based, multiplexing many connections per thread) does. Raising
`THREAD_POOL_SIZE` trades memory/OS thread overhead directly for more concurrent connection capacity, and is a
reasonable knob for testing or moderate concurrency, but it doesn't scale indefinitely the way a real event loop would.
Every thread also reserves its own stack (typically 8 MiB of virtual memory by default on Linux).

### Shutdown behavior

On `SIGINT`/`SIGTERM` the server stops accepting new connections, lets the workers drain any connections already queued,
and joins all worker threads. Workers don't check the shutdown flag mid-connection, so shutdown waits for in-flight
connections to finish: an idle keep-alive connection is released when its 5 second timeout fires, while a client that
keeps sending requests holds its worker until it disconnects or hits `MAX_KEEPALIVE_REQUESTS`.

### Benchmarking note

If you're benchmarking this yourself: be aware that repeated short-lived-connection benchmark runs (e.g. without
keep-alive, or against a client that doesn't reuse connections) can leave many sockets in `TIME_WAIT` on the machine
running the benchmark. Back-to-back runs, especially at high concurrency, can appear to slow down purely from ephemeral
port/`TIME_WAIT` pressure left over from the previous run rather than from anything in the server itself. Leave a short
gap between runs, or reduce concurrency/request count, for more comparable results. Debug builds with the sanitizers
enabled will also be considerably slower than an optimized build without them.

## Possible/Known limitations

### Protocol coverage

- No HTTPS/TLS support
- No support for `Transfer-Encoding: chunked` bodies
- No IPv6 support (the listening socket is `AF_INET`/`sockaddr_in` only)
- Only `GET`, `POST`, `PUT`, and `DELETE` are recognized; there's no `HEAD`, `OPTIONS`, `PATCH`, etc. An unrecognized
  method gets a `404`, not a `405`/`501`, and a known path requested with the wrong method also gets a `404` rather than
  `405`
- `Expect: 100-continue` is ignored (no interim `100 Continue` response), so some clients may pause briefly before
  sending large bodies
- No validation that HTTP/1.1 requests include a `Host` header, which the spec requires
- No HTTP pipelining support: if a single `read()` returns bytes belonging to a second, already-pipelined request past
  the end of the current one's body, those extra bytes are never carried forward. The next loop iteration discards the
  buffer and reads fresh from the socket, so a pipelining client can see corrupted or missing responses
- Only `\r\n\r\n` is recognized as the end-of-headers delimiter; a client sending bare `\n` line endings will stall
  until `MAX_HEADER_SIZE` is hit and get a `400`
- Handlers can't set response headers (see "Writing handlers")

### Header and request parsing

- Header lookups (`Content-Type`, `Content-Length`, `Connection`) search for the header name as a raw substring anywhere
  in the buffer rather than anchoring to the start of a line, and the search spans the whole buffer including the
  request body. A request containing an unrelated header whose name ends in one of these (e.g.
  `X-Original-Content-Type:`), or a body that happens to contain such text, can be misread as the real header
- Header name and value matching is case-sensitive, while HTTP header names and common token values (e.g.
  `Content-Type`, `keep-alive`) are case-insensitive per spec; differently-cased input from a client is not recognized
- `Content-Type` values with parameters (e.g. `application/json; charset=utf-8`) don't match any of the known exact
  strings and fall back to `CONTENT_TYPE_NONE`, even though this is a very common form in real requests
- Header parsing assumes exactly one space after the colon (e.g. `"Content-Length: "`); a spec-legal header with no
  space or extra spaces after the colon either isn't found or breaks value parsing
- Header values (HTTP method, `Content-Type`, `Connection`, HTTP version) and the request URI are copied into fixed-size
  buffers with truncation, so they can't overflow, but an oversized value is silently truncated rather than rejected. In
  practice a truncated value just fails to match anything (a truncated URI results in a `404`), but there is no
  dedicated `414`/`431` response
- If the client disconnects (or times out) before sending the full declared body, the handler is still invoked with
  whatever partial body was received, with `body_length` set to the number of bytes actually read; there is no `400` for
  an incomplete body
- No URL decoding or query-string parsing: the request path is matched literally, so `%20`-style escapes and
  `?key=value` query strings aren't handled (a path with a query string won't match its route)
- A route's declared `request_body_content_type` is stored at registration but never validated against the incoming
  request's actual `Content-Type`

### Routing and API

- Routing is exact-match only: no path parameters (`/users/:id`) or wildcards
- The route table is a process-wide global, so only one server can run per process
- `register_route` is not thread-safe; all routes must be registered before `start_server()` begins accepting
  connections
- No access/request logging

### Robustness and resources

- `write()` calls (in `write_http_header`, `write_error_response`, and the response body writes in
  `handle_client_connection`) aren't checked for partial writes; `write()` may send fewer bytes than requested, which
  today would silently truncate a response and desync a kept-alive connection
- Only an idle timeout is enforced (`SO_RCVTIMEO`), and it resets on every successful `read()` no matter how few bytes
  were read; a client that trickles in a byte or two just before each timeout expires can hold a worker thread
  indefinitely without ever completing a request (a Slowloris-style pattern). There's no overall wall-clock deadline for
  finishing a request's headers or body
- A fixed pool of `THREAD_POOL_SIZE` (1024) OS threads is spawned unconditionally at startup rather than using an
  event-driven (e.g. `epoll`) model, which is comparatively heavy on memory/OS resources and isn't adjustable at runtime
- Concurrent persistent (keep-alive) connections are capped by `THREAD_POOL_SIZE`; see "Concurrency model and its
  limits" above
- A keep-alive connection's read buffer is never shrunk back down after growing to accommodate a large request body, so
  a connection that sent one large request early on keeps that memory allocated for its full lifetime