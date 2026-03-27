#pragma once

#include "Poller.h"
#include "TimeStamp.h"

#include <vector>
#include <sys/epoll.h>

/**
 * 基于 epoll 的 I/O 复用器实现。
 *
 * 封装了 epoll 的典型使用流程：
 *  - epoll_create 创建 epoll 实例
 *  - epoll_ctl 进行 add / mod / del 操作
 *  - epoll_wait 等待 I/O 事件发生
 *
 * 一个 EventLoop 内部通常持有一个 EPollPoller 实例，
 * 只在所属 EventLoop 所在的 I/O 线程中使用，不保证多线程安全。
 */

class Channel;

/**
 * EPollPoller 负责把 Channel 关注的事件注册到 epoll，
 * 并在有事件发生时把活跃的 Channel 分发回 EventLoop。
 */
class EPollPoller : public Poller
{
public:
    /// @param loop 该 Poller 所属的 EventLoop（线程归属由 EventLoop 决定）
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    /**
     * 等待 I/O 事件并填充活跃的 Channel 列表。
     *
     * @param timeoutMS epoll_wait 的超时时间，单位：毫秒
     * @param activeChannels 输出参数，保存本次返回的活跃 Channel 列表
     * @return 本次 epoll_wait 返回的时间戳
     */
    TimeStamp poll(int timeoutMS, ChannelList *activeChannels) override;

    /**
     * 在 epoll 中添加或更新一个 Channel。
     *
     * 只应在所属 EventLoop 的 I/O 线程中调用。
     */
    void updateChannel(Channel *channel) override;

    /**
     * 从 epoll 中移除一个 Channel。
     *
     * Channel 通常由 EventLoop / TcpConnection 生命周期管理，
     * 移除时要求该 fd 已不再关注任何事件。
     */
    void removeChannel(Channel *channel) override;

private:
    /// 初始 events_ 容量，避免频繁扩容
    static const int kInitEventListSize = 16;

    /**
     * 根据 epoll_wait 返回的事件，将对应的 Channel 填入 activeChannels。
     */
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    /**
     * 统一封装 epoll_ctl 的 add / mod / del 操作。
     */
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    /// epoll 实例的文件描述符
    int epollfd_;
    /// 暂存 epoll_wait 返回的事件数组，循环复用
    EventList events_;
};