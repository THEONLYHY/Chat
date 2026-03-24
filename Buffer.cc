#include "Buffer.h"

#include <sys/uio.h> //readv

/**
 * struct iovec {
 *      void *iov_base; //Startring address 
        size_t iov_len; // Number of bytes to transfer
 * };
 */

/** 
 * 从fd上读取数据，Poller工作在LT模式
 *  Buffer缓冲区是有大小的，但是从fd上读数据的时候，不知道数据的大小
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; //栈上的空间, 64k = 64 * 1024

    struct iovec vec[2];
    // buffer底层缓冲区剩余的可写空间大小
    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0){
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据
    {
        // 原来的写满了
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}