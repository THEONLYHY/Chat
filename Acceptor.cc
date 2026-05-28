#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n ", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
    , idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    if (idleFd_ < 0)
    {
        LOG_FATAL("%s:%s:%d open /dev/null err:%d \n ", __FILE__, __FUNCTION__, __LINE__, errno);
    }

    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback([this](TimeStamp) {
        handleRead();
    });
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    if (idleFd_ >= 0)
    {
        ::close(idleFd_);
    }
}

void Acceptor::setListenBacklog(int backlog)
{
    acceptSocket_.setListenBacklog(backlog);
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    while (true)
    {
        InetAddress peerAddr;
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0)
        {
            if (newConnectionCallback_)
            {
                newConnectionCallback_(connfd, peerAddr);
            }
            else
            {
                ::close(connfd);
            }
            continue;
        }

        int savedErrno = errno;
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
        {
            break;
        }
        if (savedErrno == EINTR || savedErrno == ECONNABORTED)
        {
            continue;
        }

        LOG_ERROR("%s:%s:%d accept err:%d \n ", __FILE__, __FUNCTION__, __LINE__, savedErrno);
        if (savedErrno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d socket reached limit err:%d \n ", __FILE__, __FUNCTION__, __LINE__, savedErrno);
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
            if (idleFd_ >= 0)
            {
                ::close(idleFd_);
            }
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            if (idleFd_ < 0)
            {
                LOG_ERROR("%s:%s:%d reopen /dev/null err:%d \n ", __FILE__, __FUNCTION__, __LINE__, errno);
            }
        }
        break;
    }
}
