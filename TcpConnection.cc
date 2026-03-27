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
<<<<<<< HEAD
            loop_->runInLoop([this, msg = buf](){
                sendInLoop(msg.c_str(), msg.size());    
            });
        }
=======
            // 直接在lambda里面用buf初始化(init-capture)。需要c++14标准
            std::string msg(buf);
            loop_->runInLoop([this, msg](){
                sendInLoop(msg.c_str(), msg.size());    
            });
        }
    } 
}

/**
 * 发送数据，应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
 */
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len;
    bool faultError = false;
    // 之前shutdown，不能再进行发送
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0){
            remaining = len - nwrote;
            // 一次性把数据全部发送完成
            if (remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {   
            nwrote = 0;
            if (errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }
    }
    // 数据没有发送完， 剩下数据保存到缓冲区中，然后给 channel_注册epollout事件,
    // epollout事件也就是poller一发现tcp缓冲区有空间，就会通知sock-channel，调用writecallback_回调
    // 也就是调用TcpConnection::handleWrite把缓冲区中数据全部发送完成
    if (!faultError && remaining > 0){
        ssize_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMark_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),oldLen+remaining
            ));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()){
            channel_->enableWriting(); //注册channel写事件，否则poller不会给channel通知epollout
        }
>>>>>>> main
    }
}

void TcpConnection::shutdown()
{
<<<<<<< HEAD

}
void TcpConnection::handleRead(TimeStamp receiveTime)
=======
    if (state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop([this](){ shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 
    {
        socket_->shutdownWrite(); // 关闭写端
    }
}



// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的epollin事件

    //
    connectionCallback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
}

        void TcpConnection::handleRead(TimeStamp receiveTime)
>>>>>>> main
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
<<<<<<< HEAD
            if (outputBuffer_.readableBytes() == 0)
            {
=======
            //缓冲区无可读空间
            if (outputBuffer_.readableBytes() == 0)
            {
                // 要关闭监听 
>>>>>>> main
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

<<<<<<< HEAD
void TcpConnection::sendInLoop(const void *message, size_t len)
{

}

void TcpConnection::shutdownInLoop()
{

}
=======
>>>>>>> main
