#include "EventLoop.h"
#include "Logger.h"
#
#include <atomic>
#include <chrono>
#include <thread>
#
int main()
{
    EventLoop loop;

    // 统计回调被执行的次数
    std::atomic_int callCount{0};

    // 在另一个线程里，不断往 EventLoop 投递任务
    std::thread worker([&]() {
        for (int i = 0; i < 5; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // 注意：这里是在非 I/O 线程里调用 runInLoop，
            // 会自动退化成 queueInLoop + wakeup()
            loop.runInLoop([&]() {
                int c = ++callCount;
                LOG_INFO("runInLoop callback executed, count=%d", c);

                // 第 5 次时请求退出事件循环
                if (c >= 5)
                {
                    loop.quit();
                }
            });
        }
    });

    // 主线程进入事件循环，等待 worker 线程投递任务并唤醒
    loop.loop();

    worker.join();
    return 0;
}

