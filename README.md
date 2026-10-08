# http_server

A small HTTP/1.1 server library written from scratch in C23 as a learning project. It provides exact-match routing,
keep-alive connections, case-insensitive header parsing, request body parsing with size limits, and a fixed thread
pool. It is built as a static library (`httpserver`) plus an example executable (`dashboard_server`) and two test
executables.

## Project layout

```
include/httpserver.h        Public API (also declares the internal string/parser helpers, see below)
src/server.c                Sockets, accept loop, worker threads, connection handling, response writing
src/router.c                Route table, request-line and header parsing
src/utils/str_functions.c   String helpers and the StrSpan (pointer + length) helpers
src/utils/buffer.{c,h}      Growable per-connection read buffer
src/utils/queue.{c,h}       Blocking, fixed-size queue of accepted connections (mutex + condition variables)
example/main.c              `dashboard_server`, a demo that registers a few routes
tests/                      Unit tests for the parser and string helpers
www/test.html               Static page served by the example
```

## Building

Requires CMake 4.1+ and a C23-capable compiler (GCC 13+ or Clang 17+ recommended; the code uses `nullptr`, `constexpr`,
`thread_local`, and `bool` as keywords). Linux/POSIX only (it uses `pthread`, BSD sockets and `sigaction`). It builds in
both CMake's default `gnu23` mode and a strict `-std=c23`: `server.c` defines `_POSIX_C_SOURCE` itself so the POSIX
declarations it needs are visible either way.

```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

This produces the `httpserver` static library, the `dashboard_server` example, and the `test_str_functions` and
`test_parser` test executables.

**Strict warnings and sanitizers are on by default.** `CMakeLists.txt` compiles with
`-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror` and links with `-fsanitize=address,undefined`. Two
consequences:

- Any warning fails the build, including unused parameters and implicit signed/unsigned conversions.
- The ASan/UBSan runtimes must be installed, or linking fails with `cannot find libasan.so` / `libubsan.so`. On
  Fedora/RHEL: `sudo dnf install libasan libubsan`. On Debian/Ubuntu: `sudo apt install libasan8 libubsan1` (the version
  must match your GCC).

To build without sanitizers, remove the `-fsanitize=...` lines from `add_compile_options` and `add_link_options` in
`CMakeLists.txt`.

## Running the tests

From the build directory:

```bash
ctest --output-on-failure
```

There are two suites, registered with CTest as `str_functions` and `parsing_functions`:

| Executable           | Tests | Covers                                                                                  |
|----------------------|-------|-----------------------------------------------------------------------------------------|
| `test_str_functions` | 67    | `get_length`, `string_compare`, `string_copy`, `find_str_in_str`, lowercase helpers, `StrSpan` helpers |
| `test_parser`        | 61    | Method, URI (including the too-long check), version, `Content-Type`, `Content-Length` and `Connection` parsing |

Each test runs in its own forked process (`tests/test_helpers.h`), so a crash or sanitizer failure in one test is
reported as a failure of that test instead of taking down the whole suite. The parser tests cover case-insensitive
header names and values, missing or extra whitespace after the colon, `charset` parameters, headers that merely *end*
in a known name (e.g. `X-Original-Content-Type`), header-like text inside the body, oversized values, and
conflicting duplicate headers. The tests exercise the parsing functions only; there are no socket-level or
end-to-end tests.

## Running the example

Run it from inside `cmake-build-debug` (the static page route opens `../www/test.html` relative to the working
directory):

```bash
./dashboard_server
```

The server listens on port `8080` on all IPv4 interfaces (`INADDR_ANY`). Routes registered in `example/main.c`:

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

`start_server()` blocks until the server is shut down by `SIGINT`/`SIGTERM`. It returns `0` after a clean shutdown and
`-1` if the socket could not be created, configured, bound or put into listening mode (the socket is closed again on
those failures). It also calls `route_cleanup()`
on shutdown, so the route table is freed for you.

### Writing handlers

A handler has the signature `Response (*)(const Request*)`. The `Request` contains `method`, `uri`, `content_type`,
`body`, and `body_length`. Things to keep in mind:

- **Response body lifetime.** The server writes the response *after* your handler returns and never frees the body, so
  the body must still be valid at that point. Use string literals, `static`/`thread_local` buffers (as the example
  does), or point at `request->body` to echo it back. A `malloc`'d body would leak, since there is no cleanup hook.
- **Request body lifetime.** `request->body` points into the connection's read buffer. It is valid for the duration of
  the handler call (and while the response is written), but not afterwards; copy it if you need to keep it.
- **Handlers run concurrently** on the worker threads, so they must be thread-safe. That is why the example uses
  `thread_local` buffers rather than plain `static` ones.
- **Use `body_length`, not `get_length(body)`,** for request bodies. Bodies are not guaranteed to be text and may
  contain `NUL` bytes. When the request has no `Content-Length`, `body_length` is `0`.
- **`request->content_type` is only informative.** It is parsed from the request's `Content-Type` header (media type
  and `charset=utf-8` only, see below), but the server does not check it against the content type the route was
  registered with.
- **No custom response headers.** `Response` has no header field, so handlers can't set `Location`, `Set-Cookie`, etc.
  (a `301`/`302` can't carry a `Location`).
- **Response `Content-Type`.** `TEXT_HTML`, `APPLICATION_JSON`, `APPLICATION_JSON_UTF8` (sent as
  `application/json; charset=utf-8`), `APPLICATION_XML`, `IMAGE_JPEG` and `IMAGE_PNG` are sent as the matching media
  type. `CONTENT_TYPE_NONE` sends no `Content-Type` header at all, which is what you want for an empty body such as a
  `204`.
- **Status codes with reason phrases:** 200, 201, 204, 301, 302, 400, 401, 403, 404, 405, 413, 414, 431, 500, 501. Any other
  code is sent with the reason phrase `Unknown`.
- **Register routes before calling `start_server()`.** `register_route` returns `-1` if memory allocation fails.
  Route URIs longer than `URI_MAX_LENGTH - 1` characters are truncated at registration.

### Request parsing rules

Everything below is covered by the parser tests.

- **Request line.** Must be `METHOD SP URI SP VERSION` with single spaces. Leading blank lines (`\r\n`) before the request
  line are skipped. The method is case-sensitive (`get` is not `GET`). A malformed request line yields method
  `UNKNOWN` and an empty URI, which results in a `404`.
- **Header names are case-insensitive** (`content-length`, `CONTENT-LENGTH` and `Content-Length` are the same header).
  Lookups are anchored to the start of a header line and only search the header section, so `X-Original-Content-Type`
  and text inside the body are ignored.
- **Header values** are trimmed of leading and trailing spaces and tabs, so `Content-Length:5`, `Content-Length: 5` and
  `Content-Length:   5  ` are all accepted.
- **`Content-Type`.** The media type is matched case-insensitively against `text/html`, `application/json`,
  `application/xml`, `image/jpeg` and `image/png`; parameters are accepted. `application/json` with `charset=utf-8`
  (any case, any spacing) becomes `APPLICATION_JSON_UTF8`; any other media type (or no header) becomes
  `CONTENT_TYPE_NONE`.
- **`Content-Length`.** Absent means no body. Present means it must be a non-empty string of digits that does not
  overflow `size_t`; otherwise the request gets a `400`.
- **Duplicate headers.** Repeated `Content-Length` or `Content-Type` headers are fine if every value is identical
  (case-insensitively). If they differ, `Content-Length` is treated as invalid (`400`) and `Content-Type` as
  `CONTENT_TYPE_NONE`. This guards against request-smuggling style ambiguity.
- **`Connection`.** All `Connection` headers are scanned, and each is split on commas, so `Connection: Upgrade, close`
  works. `close` always wins over `keep-alive`. With neither token, HTTP/1.1 defaults to keep-alive and HTTP/1.0 (or an
  unrecognized version) defaults to close. The version token is case-sensitive: `http/1.1` is unrecognized.

## Configuration

These are compile-time macros in `include/httpserver.h` (`GROWTH_CHUNK` lives in `src/utils/buffer.h`):

| Macro                        | Default | Meaning                                                                  |
|------------------------------|---------|--------------------------------------------------------------------------|
| `THREAD_POOL_SIZE`           | 1024    | Worker threads spawned at startup                                        |
| `MAX_CONNECTIONS`            | 2048    | Pending-connection queue size and `listen()` backlog                     |
| `KEEP_ALIVE_TIMEOUT_SECONDS` | 5       | Idle timeout (`SO_RCVTIMEO`) on each connection                          |
| `MAX_KEEPALIVE_REQUESTS`     | 1000    | Requests served on one connection before it is closed                    |
| `MAX_HEADER_SIZE`            | 16384   | Cap on the request line plus headers, including the blank line; exceeding it returns `431` |
| `MAX_REQUEST_SIZE`           | 1048576 | Cap on headers + body; exceeding it returns `413`                        |
| `URI_MAX_LENGTH`             | 256     | Request URI buffer size; URIs of 256+ characters are rejected with `414` |
| `INIT_BUFFER_SIZE`           | 1024    | Initial size of each connection's read buffer (it doubles as needed)     |
| `GROWTH_CHUNK`               | 4096    | Headroom ensured in the read buffer before each header `read()`          |
| `RESPONSE_HEADER_SIZE`       | 256     | Stack buffer for the response status line and headers                    |

`MAX_HEADER_SIZE` is exact: a header block of 16384 bytes (request line, headers and the final `\r\n\r\n`) is accepted and
one byte more is rejected, even if it arrives in a single `read()`.

### Error responses

- Unmatched route (any method or path, including an unrecognized method or a malformed request line): `404`.
- Invalid `Content-Length` (present but empty, non-numeric, overflowing, or conflicting duplicates): `400`.
- Headers too large (more than `MAX_HEADER_SIZE` bytes before the end of the headers): `431`.
- Declared body too large (headers + body over `MAX_REQUEST_SIZE`, or a length that overflows): `413`.
- Request URI of `URI_MAX_LENGTH` (256) or more characters: `414`, sent before the route lookup and before any body is
  read.

Connections are closed after any of these errors, except the `404` for an unmatched route, which is an ordinary response
and respects keep-alive. The route lookup happens before the body is read, so a request to an unknown route still has
its declared body read off the socket first.

## Concurrency model and its limits

This server uses **thread-per-connection**: each accepted connection is handed to one worker thread from a fixed-size
pool (`THREAD_POOL_SIZE`), and that thread stays dedicated to that connection for as long as it stays open. If some
`pthread_create` calls fail at startup, the server logs the failure to `stderr` and runs with the workers it managed to
start (it gives up only if none could be started). The accept loop pushes connections into a blocking ring buffer (`ConnectionQueue`) and the workers pop from it.

With HTTP keep-alive enabled, a connection can stay open for many requests in a row, which means **`THREAD_POOL_SIZE` is
a hard ceiling on the number of concurrent persistent connections the server can actively serve**, not just a throughput
knob. If more clients hold open keep-alive connections than there are worker threads, the excess connections queue (up
to `MAX_CONNECTIONS`) and wait for a worker to free up, which only happens when some other client's connection closes,
sends its `MAX_KEEPALIVE_REQUESTS`th request, or goes idle past the keep-alive timeout. If the queue itself fills up,
the accept loop blocks until a slot frees up.

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
keeps sending requests holds its worker until it disconnects or hits `MAX_KEEPALIVE_REQUESTS`. `SIGPIPE` is ignored, so
writing to a client that has gone away does not kill the process.

### Console output

The server prints `Server listening on port <port>` at startup and `Server shutting down` on exit. It also prints
`Failed to handle client connection` when a request is rejected while reading its body (an invalid `Content-Length`
`400`, or a `413`) or when a buffer allocation fails. The "headers too large" `431` and the `414` do not print
anything. There is no access/request logging.

### Benchmarking note

If you're benchmarking this yourself: be aware that repeated short-lived-connection benchmark runs (e.g. without
keep-alive, or against a client that doesn't reuse connections) can leave many sockets in `TIME_WAIT` on the machine
running the benchmark. Back-to-back runs, especially at high concurrency, can appear to slow down purely from ephemeral
port/`TIME_WAIT` pressure left over from the previous run rather than from anything in the server itself. Leave a short
gap between runs, or reduce concurrency/request count, for more comparable results. Debug builds with the sanitizers
enabled will also be considerably slower than an optimized build without them.

## Internal helpers exposed in `httpserver.h`

The public header also declares the library's internal helpers so the tests can call them directly: `get_length`,
`string_compare`, `string_copy`, `find_str_in_str`, `string_to_lower`, `char_to_lower`, the `StrSpan` functions
(`span_from_string`, `span_equals`, `span_trim`, `next_delimited`), `find_route`, `parse_uri`, `uri_too_long`,
`parse_http_method`,
`parse_content_type`, `parse_content_length`, `parse_http_version`, `parse_keep_alive`, and `route_cleanup`. They are
not a stable API. Notably, the `parse_*` functions expect a NUL-terminated buffer holding the raw request.

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
- No validation that HTTP/1.1 requests include a `Host` header, which the spec requires, and no rejection of unknown
  HTTP versions (the version only influences the keep-alive default). Responses are always sent as `HTTP/1.1`, even to
  an HTTP/1.0 client; RFC 9112 allows this, and the `Connection` header is still set correctly for HTTP/1.0 keep-alive
- No HTTP pipelining support: if a single `read()` returns bytes belonging to a second, already-pipelined request past
  the end of the current one's body, those extra bytes are never carried forward. The next loop iteration discards the
  buffer and reads fresh from the socket, so a pipelining client can see corrupted or missing responses
- Only `\r\n\r\n` is recognized as the end-of-headers delimiter, and only `\r\n` as a line ending; a client sending bare
  `\n` line endings will stall until `MAX_HEADER_SIZE` is hit and get a `431`
- Handlers can't set response headers (see "Writing handlers")

### Header and request parsing

- Only `Content-Type`, `Content-Length` and `Connection` are interpreted; no other request header is exposed to
  handlers
- Of the `Content-Type` parameters, only `charset=utf-8` is recognized (and only for `application/json`). Other media
  types, such as `text/plain`, `multipart/form-data` or `application/x-www-form-urlencoded`, map to `CONTENT_TYPE_NONE`
- Header parsing is lenient about obsolete or odd syntax: header lines without a colon are skipped, and there is no
  validation of header-name characters or of header value contents
- The request URI is limited to `URI_MAX_LENGTH - 1` characters; a longer one gets a `414`. An oversized method or
  `Content-Type` token simply fails to match anything
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
  `handle_client_connection`) aren't checked for partial writes or errors; `write()` may send fewer bytes than
  requested, which today would silently truncate a response and desync a kept-alive connection
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