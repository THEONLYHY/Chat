## Context

The current code is a small muduo-style reactor library in the global namespace. It already has `EventLoop`, `Channel`, `Poller`, `EventLoopThreadPool`, `Socket`, `Acceptor`, `TcpConnection`, and `TcpServer` types, but the core server path is incomplete: `TcpServer::newConnection()` is empty, `TcpServer` does not keep a `ConnectionMap`, the constructor accidentally forces `kReusePort`, `started_` is not initialized, `Socket::accept()` passes an uninitialized `socklen_t`, and `Acceptor` does not implement the standard idle-fd recovery path for `EMFILE`.

The build baseline also needs to be narrowed before runtime work is meaningful. `CMakeLists.txt` currently uses `aux_source_directory(.)`, which pulls `muduo_server.cc` into the shared library and references example sources under `examples/` even though that directory is absent. This change first makes the library source list explicit, then adds a minimal `examples/echo_server.cc` that uses the current global class names and callback signatures.

## Goals / Non-Goals

**Goals:**

- Produce a clean CMake build for the `chat` shared library and a minimal echo server executable.
- Make listening sockets and accepted sockets nonblocking and close-on-exec.
- Make `Acceptor` robust enough to accept new clients, dispatch them through `NewConnectionCallback`, close unhandled fds, and recover from `EMFILE` with an idle fd.
- Make `TcpServer` own active `TcpConnectionPtr` instances, assign connections through `EventLoopThreadPool::getNextLoop()`, wire callbacks, and remove connections on close.
- Make `TcpConnection` safe for normal echo usage across loop threads by guarding optional callbacks, preserving object lifetime for queued sends, and removing its channel during destruction.
- Verify the flow with an echo server and manual smoke commands covering connect, send/receive, close, and multi-thread dispatch.

**Non-Goals:**

- Do not migrate the API into `muduo::net` or reorganize directories.
- Do not add `TimerQueue`, `TcpClient`, `Connector`, install/export targets, packaging, or CI.
- Do not change the public callback typedefs in `Callbacks.h`.
- Do not replace the current `Buffer` reader/writer index model.
- Do not compile the existing `muduo_server.cc` reference/demo into the `chat` library.

## Decisions

1. **Use an explicit library source list instead of directory-wide discovery.**

   `CMakeLists.txt` will list the core `.cc` files that belong in the `chat` library and exclude `muduo_server.cc`. This avoids accidental multiple-main or demo-code linkage while keeping the current flat source layout. The alternative was to keep `aux_source_directory(.)` and filter files afterward, but that leaves future unrelated `.cc` files vulnerable to being linked into the library by accident.

2. **Add a minimal echo example using the current API surface.**

   `examples/echo_server.cc` will include the existing headers directly, construct `EventLoop`, `InetAddress`, and `TcpServer`, set thread count to two, and echo `Buffer::retrieveAllAsString()` back through `TcpConnection::send()`. This validates the server core without introducing namespace migration or a new application framework. Missing historical examples should be removed from the default build or added only if their sources exist.

3. **Keep `Acceptor::NewConnectionCallback(int, const InetAddress&)` unchanged.**

   The accept path will fix `Socket::accept()` internally by initializing `socklen_t` and using `accept4(..., SOCK_NONBLOCK | SOCK_CLOEXEC)`. `Acceptor` will open `/dev/null` into `idleFd_`, close it in the destructor, and use the standard close/reopen sequence when `EMFILE` prevents accepting a client. This keeps existing `TcpServer` integration simple and avoids widening the callback contract.

4. **Let `TcpServer` be the owner of live connections.**

   `TcpServer` will add `ConnectionMap connections_`, create each connection with a name derived from `name_`, `ipPort_`, and `nextConnId_`, set connection/message/write/close callbacks, store the shared pointer, and schedule `TcpConnection::connectEstablished()` on the selected I/O loop. On close, `TcpServer::removeConnection()` will marshal deletion back to the base loop, erase from `connections_`, then schedule `TcpConnection::connectDestroyed()` on the connection loop. This mirrors the existing reactor ownership model and prevents a connection from being destroyed while its channel is still registered.

5. **Guard optional callbacks and queued operations in `TcpConnection`.**

   Callback invocations will either check for a non-empty callback or rely on local default callbacks, so a user can run the echo example without setting every optional hook. Cross-thread `send()` will capture both the message string and a `TcpConnectionPtr` by value before queuing work, preventing a queued lambda from dereferencing a destroyed `this`. `connectDestroyed()` will disable all events and call `Channel::remove()` so the `Poller` no longer holds a stale channel after close.

## Risks / Trade-offs

- `CMAKE_CXX_FLAGS` currently uses `-std=c++11` -> queued lambda code must stay C++11-compatible, using local variables captured by value rather than C++14 init-capture.
- Manual `nc` smoke tests require a known port and a server process that must be stopped after verification -> tasks should include explicit start, connect, and cleanup commands.
- `EMFILE` behavior is hard to reproduce in a normal smoke test -> implement the standard idle-fd path and rely on code review plus build verification for that branch.
- Existing code uses global class names and a flat source layout -> this keeps the change small, but the resulting example and CMake remain intentionally non-installable and project-local.
- Connection removal crosses event loops -> all map mutation must happen on the base loop, while channel destruction must happen on the owning I/O loop.
