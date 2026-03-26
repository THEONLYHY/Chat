#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
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
    , idleFd_(-1)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start() Acceptor.listen 有新用户的连接，执行一个回调 (connfd => channel => subloop)
    //acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
     acceptChannel_.setReadCallback([this](TimeStamp){
        handleRead();
    });
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();

}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading();
}



void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            //轮询找到subloop，唤醒， 分发当前的新客户端的Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n ", __FILE__, __FUNCTION__, __LINE__, errno);
        //超上限了。1.调整上限
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d  socket reached limit err:%d \n ", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}