#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * Channel 表示一个文件描述符及其关注的事件和回调。
 *
 * 在 Reactor 模型中：
 *  - 一个 Channel 只负责“事件分发”：保存 fd、感兴趣的事件，以及对应的回调；
 *  - 不拥有 fd 的生命周期（fd 由上层对象负责关闭）；
 *  - 由 EventLoop / Poller 负责把 Channel 注册到 I/O 复用器里。
 */
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(TimeStamp)>;

    /**
     * @param loop 该 Channel 所属的 EventLoop
     * @param fd   需要监听的文件描述符（socket / eventfd 等）
     */
    Channel(EventLoop *loop, int fd);
    ~Channel();

    /**
     * 当 fd 在 Poller 中变为活跃后，由 EventLoop 调用此接口分发具体事件。
     *
     * @param receiveTime Poller 返回的时间戳，用于给读回调传递精确时间
     */
    void handleEvent(TimeStamp receiveTime);

    /// 设置各种事件对应的回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /**
     * 绑定一个 shared_ptr 所指对象的生命周期。
     *
     * 通常在 TcpConnection 中调用，保证当 Channel 处理事件时，
     * 对应的连接对象还存活，避免回调过程中访问已析构对象。
     */
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }
    bool isNonEvents() const { return events_ == kNoneEvent; }

    /// 开启 / 关闭对读写事件的关注，并通知 Poller 更新
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    /// 供 Poller 内部使用的索引（如在数组中的位置），对上层透明
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    /// one loop per thread：一个 Channel 只隶属于一个 EventLoop
    EventLoop* ownerLoop() { return loop_; }

    /**
     * 从所属 EventLoop / Poller 中移除当前 Channel。
     *
     * 通常在上层对象准备销毁 fd 之前调用。
     */
    void remove();

private:
    /// 把当前 Channel 的事件变更同步到 Poller
    void update();

    /// 内部真正执行事件分发的实现，配合 tie_ 进行生命周期保护
    void handleEventwithGuard(TimeStamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 所属的事件循环（线程归属）
    const int fd_;    // 被监听的文件描述符
    int events_;      // 注册到 Poller 中感兴趣的事件集合
    int revents_;     // Poller 返回的实际发生的事件集合
    int index_;       // 由 Poller 维护的内部索引

    std::weak_ptr<void> tie_; // 用于保证 handleEvent 期间上层对象存活
    bool tied_{false};        // 是否已经调用过 tie()

    // 由于 Channel 能够获知 fd 最终发生的具体事件（revents_），
    // 因此由它来调用对应的回调函数。
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};