#pragma once

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
        , listenBacklog_(1024)
        {}
    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    void setListenBacklog(int backlog);
    int listenBacklog() const { return listenBacklog_; }
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;
    int listenBacklog_;
};
