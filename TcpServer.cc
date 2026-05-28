#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <errno.h>
#include <cstdio>
#include <functional>
#include <strings.h>
#include <sys/socket.h>

namespace
{

InetAddress getLocalAddr(int sockfd)
{
    sockaddr_in localAddr;
    bzero(&localAddr, sizeof localAddr);
    socklen_t addrlen = sizeof localAddr;
    if (::getsockname(sockfd, (sockaddr*)&localAddr, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::getLocalAddr getsockname error:%d \n", errno);
    }
    return InetAddress(localAddr);
}

} // namespace

EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , writeCompleteCallback_()
    , threaInitCallback_()
    , started_(0)
    , nextConnId_(1)
    , connections_()
{
    // 两个占位符_1 : ip, _2 : peerAddr
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    
}

// 设置底层subloop的数量
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::setListenBacklog(int backlog)
{
    acceptor_->setListenBacklog(backlog);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0)
    {
        threadPool_->start(threaInitCallback_); 
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    InetAddress localAddr(getLocalAddr(sockfd));
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
