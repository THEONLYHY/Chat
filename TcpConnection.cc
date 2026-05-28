#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

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
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64 * 1024 * 1024)
    , maxOutputBufferBytes_(0)
{
    channel_->setReadCallback([this](TimeStamp ts) {
        handleRead(ts);
    });
    channel_->setWriteCallback([this]() {
        handleWrite();
    });
    channel_->setCloseCallback([this]() {
        handleClose();
    });
    channel_->setErrorCallback([this]() {
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
    if (state_ != kConnected)
    {
        return;
    }

    if (loop_->isInLoopThread())
    {
        sendInLoop(buf.c_str(), buf.size());
    }
    else
    {
        std::string msg(buf);
        TcpConnectionPtr self(shared_from_this());
        loop_->runInLoop([self, msg]() {
            self->sendInLoop(msg.c_str(), msg.size());
        });
    }
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = static_cast<ssize_t>(len);
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = static_cast<ssize_t>(len) - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            int savedErrno = errno;
            if (savedErrno != EWOULDBLOCK && savedErrno != EAGAIN && savedErrno != EINTR)
            {
                LOG_ERROR("TcpConnection::sendInLoop errno:%d", savedErrno);
                faultError = (savedErrno == EPIPE || savedErrno == ECONNRESET);
            }
        }
    }

    if (faultError)
    {
        handleClose();
        return;
    }

    if (remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        size_t newLen = oldLen + static_cast<size_t>(remaining);
        if (newLen >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), newLen));
        }

        outputBuffer_.append(static_cast<const char*>(data) + nwrote, static_cast<size_t>(remaining));
        if (maxOutputBufferBytes_ > 0 && outputBuffer_.readableBytes() > maxOutputBufferBytes_)
        {
            LOG_ERROR("TcpConnection::sendInLoop output buffer too large: %lu > %lu",
                      outputBuffer_.readableBytes(), maxOutputBufferBytes_);
            forceCloseInLoop();
            return;
        }

        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        TcpConnectionPtr self(shared_from_this());
        loop_->runInLoop([self]() {
            self->shutdownInLoop();
        });
    }
}

void TcpConnection::forceClose()
{
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        TcpConnectionPtr self(shared_from_this());
        loop_->queueInLoop([self]() {
            self->forceCloseInLoop();
        });
    }
}

void TcpConnection::forceCloseInLoop()
{
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        handleClose();
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    if (connectionCallback_)
    {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed()
{
    if (state_ != kDisconnected)
    {
        setState(kDisconnected);
        if (connectionCallback_)
        {
            connectionCallback_(shared_from_this());
        }
    }
    if (!channel_->isNonEvents())
    {
        channel_->disableAll();
    }
    channel_->remove();
}

void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        if (messageCallback_)
        {
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        if (savedErrno == EWOULDBLOCK || savedErrno == EAGAIN || savedErrno == EINTR)
        {
            return;
        }
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead errno:%d", savedErrno);
        handleError();
        if (savedErrno == ECONNRESET || savedErrno == EPIPE)
        {
            handleClose();
        }
    }
}

void TcpConnection::handleWrite()
{
    if (!channel_->isWriting())
    {
        LOG_ERROR("TcpConncetion fd = %d is down, no more writing \n", channel_->fd());
        return;
    }

    int saveErrno = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
    if (n > 0)
    {
        outputBuffer_.retrieve(static_cast<size_t>(n));
        if (outputBuffer_.readableBytes() == 0)
        {
            channel_->disableWriting();
            if (writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
            if (state_ == kDisconnecting)
            {
                shutdownInLoop();
            }
        }
        return;
    }

    if (saveErrno == EWOULDBLOCK || saveErrno == EAGAIN || saveErrno == EINTR)
    {
        return;
    }

    LOG_ERROR("TcpConnection::handleWrite errno:%d", saveErrno);
    if (saveErrno == EPIPE || saveErrno == ECONNRESET)
    {
        handleClose();
    }
}

void TcpConnection::handleClose()
{
    if (state_ == kDisconnected)
    {
        return;
    }

    LOG_INFO("TcpConnection::handleClose() fd=%d state=%d \n", channel_->fd(), state_.load());
    setState(kDisconnected);
    if (!channel_->isNonEvents())
    {
        channel_->disableAll();
    }

    TcpConnectionPtr connPtr(shared_from_this());
    if (connectionCallback_)
    {
        connectionCallback_(connPtr);
    }
    if (closeCallback_)
    {
        closeCallback_(connPtr);
    }
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
