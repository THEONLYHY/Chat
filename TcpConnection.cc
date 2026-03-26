#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"



static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)), 
    name_(nameArg), 
    state_(kConnecting), 
    reading_(true), 
    socket_(new Socket(sockfd)), 
    channel_(new Channel(loop, sockfd)), 
    localAddr_(localAddr), peerAddr_(peerAddr), 
    highWaterMark_(64 * 1024 * 1024) // 64M
{
    channel_->setReadCallback([this](TimeStamp ts){
            handleRead(ts);
        });
    channel_->setWriteCallback([this](){
        handleWrite();
    });
    channel_->setCloseCallback([this](){
        handleClose();
    });
    channel_->setErrorCallback([this](){
        handleError();
    });

    LOG_INFO("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

 TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
        name_.c_str(), channel_->fd(), state_.load());
}

void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            //loop_->runInLoop(std::bind(TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
            // buf如果被销毁的话，buf.c_str() 就不安全，所以要用值捕获来避免引用悬空。
            //在异步/跨线程回调中，绝不能依赖外部对象的生命周期
            loop_->runInLoop([this, msg = buf](){
                sendInLoop(msg.c_str(), msg.size());    
            });
        }
    }
}

void TcpConnection::shutdown()
{

}
void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        //
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }

}
void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    /*loop_->queueInLoop(
                    [self = shared_from_this(), cb = writeCompleteCallback_]() {
                        cb(self);       
                    } */

                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleRead");
        }
    }
    else{
        LOG_ERROR("TcpConncetion fd = %d is down, no more writing \n", channel_->fd());
    }
    
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose() fd=%d state=%d \n", channel_->fd(), state_.load());
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  //执行连接关闭的回调
    closeCallback_(connPtr);    //关闭连接的回调
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::sendInLoop(const void *message, size_t len)
{

}

void TcpConnection::shutdownInLoop()
{

}