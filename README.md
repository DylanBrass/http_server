
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

## Concurrency model and its limits

This server uses **thread-per-connection**: each accepted connection is handed to one worker thread from a fixed-size pool (`THREAD_POOL_SIZE`), and that thread stays dedicated to that connection for as long as it stays open.

With HTTP keep-alive enabled, a connection can stay open for many requests in a row, which means **`THREAD_POOL_SIZE` is a hard ceiling on the number of concurrent persistent connections the server can actively serve**, not just a throughput knob. If more clients hold open keep-alive connections than there are worker threads, the excess connections queue (up to `MAX_CONNECTIONS`) and wait for a worker to free up — which only happens when some other client's connection closes, sends its `MAX_KEEPALIVE_REQUESTS`th request, or goes idle past the keep-alive timeout.

This is an intentional simplicity tradeoff for a from-scratch learning project, not a bug: thread-per-connection is straightforward to reason about, but doesn't scale to large numbers of concurrent (mostly idle) persistent connections the way an event-loop model (`epoll`/`kqueue`-based, multiplexing many connections per thread) does. Raising `THREAD_POOL_SIZE` trades memory/OS thread overhead directly for more concurrent connection capacity, and is a reasonable knob for testing or moderate concurrency — but it doesn't scale indefinitely the way a real event loop would.

If you're benchmarking this yourself: be aware that repeated short-lived-connection benchmark runs (e.g. without keep-alive, or against a client that doesn't reuse connections) can leave many sockets in `TIME_WAIT` on the machine running the benchmark. Back-to-back runs, especially at high concurrency, can appear to slow down purely from ephemeral port/`TIME_WAIT` pressure left over from the previous run rather than from anything in the server itself — leave a short gap between runs, or reduce concurrency/request count, for more comparable results.

## Possible/Known limitations

- No HTTPS/TLS support
- No support for `Transfer-Encoding: chunked` bodies
- No IPv6 support (the listening socket is `AF_INET`/`sockaddr_in` only)
- Only `GET`, `POST`, `PUT`, and `DELETE` are recognized; there's no `HEAD`, `OPTIONS`, `PATCH`, etc.
- Routing is exact-match only — no path parameters (`/users/:id`) or wildcards
- A route's declared `request_body_content_type` is stored at registration but never validated against the incoming request's actual `Content-Type`
- No URL decoding or query-string parsing — the request path is matched literally, so `%20`-style escapes and `?key=value` query strings aren't handled
- Concurrent persistent (keep-alive) connections are capped by `THREAD_POOL_SIZE` — see "Concurrency model and its limits" above. A fixed pool of `THREAD_POOL_SIZE` (1024) OS threads is spawned unconditionally at startup rather than using an event-driven (e.g. epoll) model, which is comparatively heavy on memory/OS resources and isn't adjustable at runtime
- A keep-alive connection's read buffer is never shrunk back down after growing to accommodate a large request body, so a connection that sent one large request early on keeps that memory allocated for its full lifetime
- `register_route` is not thread-safe; all routes must be registered before `start_server()` begins accepting connections
- No access/request logging
- A few header-value fields (HTTP method, `Content-Type`, `Connection`, `HTTP` version token) are parsed into small fixed-size stack buffers sized for well-formed input; malformed or oversized values aren't yet bounds-checked against these buffers, so this needs hardening before being exposed to untrusted clients
- `write()` calls (in `write_http_header`, `write_error_response`, and the response body writes in `handle_client_connection`) aren't checked for partial writes; `write()` may send fewer bytes than requested, which today would silently truncate a response and desync a kept-alive connection
- Only an idle timeout is enforced (`SO_RCVTIMEO`), and it resets on every successful `read()` no matter how few bytes were read; a client that trickles in a byte or two just before each timeout expires can hold a worker thread indefinitely without ever completing a request (a Slowloris-style pattern) — there's no overall wall-clock deadline for finishing a request's headers or body
- No validation that HTTP/1.1 requests include a `Host` header, which the spec requires
- Header lookups (`Content-Type`, `Content-Length`, `Connection`) search for the header name as a raw substring anywhere in the buffer rather than anchoring to the start of a line; a request containing an unrelated header whose name ends in one of these (e.g. `X-Original-Content-Type:`) can be misread as the real header
- Header name and value matching is case-sensitive, while HTTP header names and common token values (e.g. `Content-Type`, `keep-alive`) are case-insensitive per spec; differently-cased input from a client is not recognized
- `Content-Type` values with parameters (e.g. `application/json; charset=utf-8`) don't match any of the known exact strings and fall back to `CONTENT_TYPE_NONE`, even though this is a very common form in real requests
- Header parsing assumes exactly one space after the colon (e.g. `"Content-Length: "`); a spec-legal header with no space or extra spaces after the colon either isn't found or breaks value parsing
- Only `\r\n\r\n` is recognized as the end-of-headers delimiter; a client sending bare `\n` line endings will stall until `MAX_HEADER_SIZE` is hit and get a `400`
- No HTTP pipelining support: if a single `read()` returns bytes belonging to a second, already-pipelined request past the end of the current one's body, those extra bytes are never carried forward — the next loop iteration discards the buffer and reads fresh from the socket, so a pipelining client can see corrupted or missing responses
