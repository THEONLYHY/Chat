## Why

The current muduo-style server codebase is not yet a complete, verifiable TCP server loop: the build baseline is unstable, accept handling is incomplete, and `TcpServer` does not fully own the connection lifecycle. This change establishes a focused core-server milestone that can build, run, accept clients, echo messages, and shut connections down cleanly.

## What Changes

- Stabilize the build baseline by explicitly listing library sources, excluding reference/demo code from the library target, and adding a minimal echo server example.
- Complete nonblocking accept behavior and `Acceptor` file descriptor exhaustion handling.
- Implement the `TcpServer` main path from accept through event-loop assignment, `TcpConnection` creation, callback wiring, connection tracking, and removal.
- Tighten `TcpConnection` callback defaults, cross-thread send lifetime safety, and channel cleanup on destruction.
- Add functional verification through an echo server workflow that exercises connect, receive, send, close, and multi-thread dispatch paths.
- Keep the public callback signatures and current global namespace API unchanged.

## Capabilities

### New Capabilities

- `muduo-server-core`: A buildable and runnable muduo-style TCP server core that accepts clients, dispatches connections to an `EventLoopThreadPool`, manages `TcpConnection` lifecycles, and supports echo/chat-style examples.

### Modified Capabilities

None.

## Impact

- Affected build files: `CMakeLists.txt`.
- Affected examples: `examples/echo_server.cc` and currently referenced example targets.
- Affected core server classes: `Socket`, `Acceptor`, `TcpServer`, and `TcpConnection`.
- Affected runtime behavior: listener accept flow, connection callback flow, message echo flow, close/removal flow, and multi-thread event-loop assignment.
- No namespace migration, install/export work, `TimerQueue`, `TcpClient`, or `Connector` work is included in this change.
