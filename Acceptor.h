#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"

#include <functional>


class EventLoop;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    void listen();
    bool listenning() const { return listenning_; }
private:
    void handleRead();

    EventLoop* loop_; // baseloop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
    int idleFd_;
};