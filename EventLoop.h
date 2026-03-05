#pragma once

#include <mutex>
#include <functional>
#include <atomic>
#include <vector>
#include <memory>

#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

/**
 * Reactor 模型中的事件循环（one loop per thread）。
 *
 * 职责：
 *  - 通过内部的 Poller 进行 I/O 复用（如 epoll）；
 *  - 分发活跃 Channel 的事件回调；
 *  - 执行其他线程投递进来的回调任务（runInLoop / queueInLoop）。
 *
 * 线程模型：
 *  - 每个线程最多持有一个 EventLoop；
 *  - 除了 runInLoop/queueInLoop 相关接口，绝大多数成员函数只能在
 *    所属 I/O 线程中调用。
 */
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    
    EventLoop();
    ~EventLoop();

    /**
     * 启动事件循环，直到调用 quit() 才会退出。
     *
     * 典型流程：
     *  - 调用 Poller::poll 等待 I/O 事件；
     *  - 处理活跃 Channel 的回调；
     *  - 执行挂起的 Functor（pendingFunctors_）。
     */
    void loop();

    /**
     * 请求退出事件循环。
     *
     * 线程安全：可从其他线程调用，会借助 wakeupFd_ 唤醒 I/O 线程。
     */
    void quit();

    /// 最近一次 Poller::poll 返回的时间戳
    TimeStamp pollReturnTime() const { return pollReturnTime_;}

    /**
     * 在当前 I/O 线程中执行回调。
     *
     * 如果调用线程就是 I/O 线程，则立即执行；
     * 否则将回调加入待执行队列，并唤醒 I/O 线程。
     */
    void runInLoop(Functor cb);

    /**
     * 将回调加入待执行队列，由 I/O 线程异步执行。
     *
     * 无论在哪个线程调用，都会排队等待在合适的时机被执行。
     */
    void queueInLoop(Functor cb);

    /**
     * 唤醒 I/O 线程（通常由其他线程调用）。
     *
     * 一般通过 eventfd/pipe 等可读事件来唤醒 Poller::poll。
     */
    void wakeup();

    /// 更新某个 Channel 在 Poller 中关注的事件
    void updateChannel(Channel * channel);

    /// 从 Poller 中移除某个 Channel
    void removeChannel(Channel * channel);

    /// 当前 Poller 是否持有该 Channel
    bool hasChannel(Channel * channel);

    /// 当前线程是否为本 EventLoop 所在的 I/O 线程
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
    /// 处理唤醒 fd 的读事件，清空可读状态
    void handleRead();

    /// 执行 pendingFunctors_ 中的所有回调
    void doPendingFunctors();
    
    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;  // 原子操作，是否正在 loop()
    std::atomic_bool quit_;     // 是否请求退出 loop
    
    const pid_t threadId_;      // 本 EventLoop 所在的线程 ID

    TimeStamp pollReturnTime_;  // 最近一次 Poller::poll 返回时间s
    std::unique_ptr<Poller> poller;  // I/O 复用器实现
    
    int wakeupFd_;                      // 用于跨线程唤醒 EventLoop 的 fd
    std::unique_ptr<Channel> wakeupChannel_; // 负责监听 wakeupFd_ 的 Channel

    ChannelList activeChannels_;        // 每次 poll 返回的活跃 Channel 列表

    std::atomic_bool callingPendingFunctors_; // 标记当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 其它线程投递进来的任务
    std::mutex mutex_;                        // 保护 pendingFunctors_
};