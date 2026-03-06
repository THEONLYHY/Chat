#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>

#include <atomic>
#include <chrono>
#include <thread>

int main()
{
    EventLoop loop;

    int efd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0)
    {
        LOG_FATAL("eventfd create failed: %d", errno);
    }

    std::atomic_int readCount{0};

    Channel ch(&loop, efd);
    ch.setReadCallback([&](TimeStamp ts) {
        uint64_t one = 0;
        ssize_t n = ::read(efd, &one, sizeof(one));
        if (n == sizeof(one))
        {
            int c = ++readCount;
            LOG_INFO("eventfd readable one=%llu at %s (count=%d)",
                     static_cast<unsigned long long>(one),
                     ts.toString().c_str(),
                     c);

            if (c >= 5)
            {
                loop.quit();
            }
        }
    });
    ch.enableReading();

    std::thread writer([&]() {
        for (int i = 0; i < 5; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            uint64_t one = 1;
            ::write(efd, &one, sizeof(one));
        }
    });

    loop.loop();

    writer.join();
    ::close(efd);
    return 0;
}

