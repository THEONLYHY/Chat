#pragma once
#include "noncopyable.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "TimeStamp.h"


#include <memory>
#include <string>
#include <atomic>

class EventLoop;
class Channel;
class Socket;



class TcpConnection : noncopyable,
                    public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    //发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();
    void forceClose();
    void setTcpNoDelay(bool on);
    void setMaxOutputBufferBytes(size_t bytes) { maxOutputBufferBytes_ = bytes; }
    size_t outputBufferBytes() const { return outputBuffer_.readableBytes(); }

    void setMessageCallback(const MessageCallback& cb) 
    { messageCallback_ = cb; }

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }
    
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark;}

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    
    
    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();


    EventLoop *loop_; // 不是baseLoop， subloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接回调
    MessageCallback messageCallback_;   // 有读写消息回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    size_t maxOutputBufferBytes_;
    
    Buffer inputBuffer_;
    Buffer outputBuffer_;

};
