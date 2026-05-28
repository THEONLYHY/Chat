## 1. Build Baseline and Echo Example

- [x] 1.1 Replace `aux_source_directory(.)` in `CMakeLists.txt` with an explicit `CHAT_SOURCES` list containing only library implementation files and excluding `muduo_server.cc`.
- [x] 1.2 Remove default executable targets for missing `examples/test_eventloop.cc` and `examples/test_runinloop.cc`, or guard them behind `if(EXISTS ...)` checks.
- [x] 1.3 Create `examples/echo_server.cc` using the current global namespace API: `EventLoop`, `InetAddress`, `TcpServer`, `Buffer::retrieveAllAsString()`, and `TcpConnection::send()`.
- [x] 1.4 Add an `echo_server` executable target linked with `chat` and `pthread`, with project headers available through `target_include_directories`.

## 2. Socket and Acceptor Stability

- [x] 2.1 Update `Socket::accept()` to initialize `socklen_t len = sizeof addr` and call `accept4(sockfd_, ..., SOCK_NONBLOCK | SOCK_CLOEXEC)`.
- [x] 2.2 Include any required headers for `accept4`, `open`, `O_RDONLY`, and `/dev/null` handling.
- [x] 2.3 Initialize `Acceptor::idleFd_` by opening `/dev/null` in the constructor and close it in the destructor.
- [x] 2.4 Implement `EMFILE` handling in `Acceptor::handleRead()` by closing `idleFd_`, accepting one pending connection, closing it, and reopening `/dev/null`.
- [x] 2.5 Preserve `Acceptor::NewConnectionCallback(int, const InetAddress&)` and close accepted sockets when no callback is installed.

## 3. TcpServer Connection Ownership

- [x] 3.1 Fix `TcpServer` construction to pass `option == kReusePort` into `Acceptor`, initialize `started_(0)`, and initialize `writeCompleteCallback_`.
- [x] 3.2 Add `ConnectionMap connections_` to `TcpServer` so live `TcpConnectionPtr` instances are owned by the server.
- [x] 3.3 Add or use a local-address lookup for each accepted socket so `TcpConnection` receives a valid `localAddr`.
- [x] 3.4 Implement `TcpServer::newConnection()` to choose an I/O loop with `threadPool_->getNextLoop()`, create a unique connection name, construct `TcpConnectionPtr`, and insert it into `connections_`.
- [x] 3.5 In `TcpServer::newConnection()`, set connection, message, write-complete, and close callbacks on the connection, then schedule `TcpConnection::connectEstablished()` on the connection loop.
- [x] 3.6 Implement `TcpServer::removeConnection()` to marshal removal back to the base loop.
- [x] 3.7 Implement `TcpServer::removeConnectionInLoop()` to erase the connection from `connections_` and schedule `TcpConnection::connectDestroyed()` on the connection's owning loop.

## 4. TcpConnection Lifecycle Safety

- [x] 4.1 Add `TcpConnection::setConnectionCallback(const ConnectionCallback&)` if it is still missing.
- [x] 4.2 Guard all optional callback invocations in `TcpConnection` so empty `connectionCallback_`, `messageCallback_`, `writeCompleteCallback_`, `highWaterMarkCallback_`, and `closeCallback_` objects are never called.
- [x] 4.3 Update cross-thread `TcpConnection::send()` to capture a `TcpConnectionPtr self = shared_from_this()` and a copied message by value before queuing `sendInLoop()`.
- [x] 4.4 Update `TcpConnection::connectDestroyed()` to set the disconnected state, disable all channel events, invoke the connection callback only when configured, and call `channel_->remove()`.
- [x] 4.5 Review `handleClose()`, `handleWrite()`, and `sendInLoop()` so close/removal and write-complete callbacks cannot access a destroyed connection.

## 5. Verification

- [x] 5.1 Configure the project with `cmake -S /home/user/03_projectes/Chat/.worktrees/codex-worktree -B /home/user/03_projectes/Chat/.worktrees/codex-worktree/build`.
- [x] 5.2 Build the project with `cmake --build /home/user/03_projectes/Chat/.worktrees/codex-worktree/build -j 4`.
- [x] 5.3 Start the echo server example and connect with `nc 127.0.0.1 <port>`.
- [x] 5.4 Send a text payload through `nc` and confirm the exact payload is echoed back.
- [x] 5.5 Disconnect the client and confirm the server stays running while the connection removal path completes.
- [x] 5.6 Run a multi-client smoke test with `setThreadNum(2)` enabled and confirm each client can connect, echo data, and disconnect.
