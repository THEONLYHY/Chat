#pragma once
#include "noncopyable.h"

#include <string>
#include <string.h>
#include <vector>
#include <algorithm>



/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

class Buffer : noncopyable
{

public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    { return writerIndex_ - readerIndex_; }

    size_t writableBytes() const
    { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const
    { return readerIndex_; }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const
    { return begin() + readerIndex_; }

    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            // 只读了可读缓冲区数据的一部分，还剩下的readerIndex_ += len -> writerIndex
            // 所以下次读从这里开始,让readerIndex后移
            readerIndex_ += len; 
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }
    //缓冲区复位
    void retrieveAll()
    { 
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }
    // 把onMessage上报的Buffer数据，转成string
    std::string retrieveAllAsString()
    { return retrieveAsString(readableBytes()); }

    std::string retrieveAsString(size_t len)
    {
        //把可读数据读取
        std::string result(peek(), len);
        retrieve(len); //对缓冲区进行复位操作
        return result;
    }   
    // buffer.size - wirterIndex
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); 
        }
    }
    // 扩容
    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            // 把可读的空间移到前面
            std::copy(begin() + readerIndex_, 
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    //把data + len添加到write
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    
    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    //从fd上读数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);
private:

    char* begin()
    { 
        // it.operator*(), vector底层数组首元素的地址,也就是数组的起始地址
        return &*buffer_.begin(); 
    }

    const char* begin() const
    { return &*buffer_.begin(); }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};