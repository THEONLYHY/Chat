## ADDED Requirements

### Requirement: Buildable core server library
The system SHALL build the `chat` shared library from explicit core server sources without compiling reference executables or missing example files into the library target.

#### Scenario: Configure succeeds from a clean build directory
- **WHEN** CMake is configured with `cmake -S /home/user/03_projectes/Chat/.worktrees/codex-worktree -B /home/user/03_projectes/Chat/.worktrees/codex-worktree/build`
- **THEN** configuration completes without errors about missing example files or multiple executable entry points in the `chat` library.

#### Scenario: Library and echo example build
- **WHEN** the build is run with `cmake --build /home/user/03_projectes/Chat/.worktrees/codex-worktree/build -j 4`
- **THEN** the `chat` shared library and echo server example executable are produced successfully.

### Requirement: Nonblocking accept path
The system SHALL accept new TCP connections using nonblocking close-on-exec sockets and SHALL provide the accepted peer address to the server connection callback path.

#### Scenario: Accepted client socket is usable by the event loop
- **WHEN** a client connects to the listening server socket
- **THEN** `Socket::accept()` returns a valid nonblocking close-on-exec connection file descriptor and stores the client address in the supplied `InetAddress`.

#### Scenario: Acceptor dispatches accepted connections
- **WHEN** `Acceptor::handleRead()` accepts a connection and a `NewConnectionCallback` is installed
- **THEN** the callback is invoked with the accepted socket file descriptor and peer address.

#### Scenario: Acceptor closes unhandled accepted sockets
- **WHEN** `Acceptor::handleRead()` accepts a connection and no `NewConnectionCallback` is installed
- **THEN** the accepted socket file descriptor is closed.

#### Scenario: Acceptor handles descriptor exhaustion
- **WHEN** accepting a connection fails with `EMFILE`
- **THEN** `Acceptor` temporarily releases its idle `/dev/null` file descriptor, accepts and closes one pending client connection, and reopens the idle file descriptor.

### Requirement: TcpServer manages connection lifecycle
The system SHALL have `TcpServer` create, own, dispatch, and remove `TcpConnection` instances for every accepted client connection.

#### Scenario: New connection is assigned to an event loop
- **WHEN** `TcpServer::newConnection()` receives an accepted socket and peer address
- **THEN** it obtains an I/O loop from `EventLoopThreadPool::getNextLoop()` and creates a `TcpConnection` bound to that loop.

#### Scenario: New connection is tracked and established
- **WHEN** a `TcpConnection` is created for an accepted client
- **THEN** `TcpServer` stores it in `connections_`, wires the connection, message, write-complete, and close callbacks, and schedules `connectEstablished()` on the connection's I/O loop.

#### Scenario: Closed connection is removed from base loop ownership
- **WHEN** a connection invokes its close callback
- **THEN** `TcpServer::removeConnection()` schedules removal on the base loop, erases the connection from `connections_`, and schedules `connectDestroyed()` on the connection's owning loop.

#### Scenario: Server startup is idempotent
- **WHEN** `TcpServer::start()` is called more than once
- **THEN** the thread pool and acceptor listen path are started only once.

### Requirement: TcpConnection callbacks and queued sends are lifetime safe
The system SHALL allow `TcpConnection` to operate safely when optional callbacks are unset and when `send()` is called from a thread other than the connection's I/O loop.

#### Scenario: Missing optional callbacks do not crash the connection
- **WHEN** a connection is established, receives a message, completes a write, crosses the high-water mark, or closes without every optional callback configured
- **THEN** the connection either invokes the configured callback or performs a no-op default path without dereferencing an empty function object.

#### Scenario: Cross-thread send keeps the connection alive
- **WHEN** `TcpConnection::send()` is called from outside the connection's I/O loop
- **THEN** the queued send work captures a `TcpConnectionPtr` and message copy by value before invoking `sendInLoop()`.

#### Scenario: Destroyed connection removes its channel
- **WHEN** `TcpConnection::connectDestroyed()` runs after a close
- **THEN** the channel is disabled and removed from its `EventLoop`/`Poller` before the connection can be destroyed.

### Requirement: Echo server demonstrates the core server loop
The system SHALL include a minimal echo server example that demonstrates accepting clients, receiving data, echoing data, and closing connections with the current global namespace API.

#### Scenario: Echo server accepts and echoes a message
- **WHEN** the echo server example is running and a client sends a text payload to `127.0.0.1` on the configured port
- **THEN** the client receives the same payload back from the server.

#### Scenario: Client disconnect does not crash the server
- **WHEN** a connected client closes the TCP connection
- **THEN** the server runs the close/removal path without crashing and removes the connection from `TcpServer` ownership.

#### Scenario: Multiple clients can use threaded dispatch
- **WHEN** the echo server runs with `setThreadNum(2)` and multiple clients connect
- **THEN** each client can connect, receive echoed messages, and disconnect successfully.
