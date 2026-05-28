#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "TimeStamp.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

void onConnection(const TcpConnectionPtr &conn)
{
    std::cout << conn->peerAddress().toIpPort() << " -> "
              << conn->localAddress().toIpPort()
              << (conn->connected() ? " connected" : " disconnected")
              << std::endl;
}

void onMessage(const TcpConnectionPtr &conn, Buffer *buffer, TimeStamp)
{
    std::string message = buffer->retrieveAllAsString();
    conn->send(message);
}

} // namespace

int main(int argc, char *argv[])
{
    uint16_t port = 6000;
    if (argc > 1)
    {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    EventLoop loop;
    InetAddress listenAddr(port, "127.0.0.1");
    TcpServer server(&loop, listenAddr, "EchoServer");

    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);
    server.setThreadNum(2);
    server.start();

    std::cout << "echo_server listening on " << listenAddr.toIpPort() << std::endl;
    loop.loop();
    return 0;
}
