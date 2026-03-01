#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

/**
 * I/O 复用器抽象基类。
 *
 * 封装了针对不同 I/O 复用实现（如 epoll、poll、select）的统一接口，
 * 由 EventLoop 持有，一个 EventLoop 只对应一个 Poller。
 *
 * 线程模型：
 *  - 只在所属 EventLoop 的 I/O 线程中使用；
 *  - 不提供跨线程并发访问的安全保证。
 */
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    /// @param loop 拥有该 Poller 的 EventLoop
    Poller(EventLoop* loop);
    virtual ~Poller();

    /**
     * 等待 I/O 事件，并返回本次等待返回的时间戳。
     *
     * @param timeoutMs 等待超时时间，单位：毫秒
     * @param activeChannels 输出参数，保存本次活跃的 Channel 列表
     */
    virtual TimeStamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

    /**
     * 在 I/O 复用器中添加或更新一个 Channel 的关注事件。
     */
    virtual void updateChannel(Channel* channel) = 0;

    /**
     * 从 I/O 复用器中移除一个 Channel。
     */
    virtual void removeChannel(Channel* channel) = 0;

    /**
     * 判断该 Poller 当前是否管理给定 Channel。
     */
    bool hasChannel(Channel * channel) const;

    /**
     * 根据当前平台/配置创建一个默认的 Poller 实例。
     */
    static Poller* newDefaultPoller(EventLoop* loop);
protected:
    /// key 为 fd，value 为对应的 Channel*
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    /// 拥有该 Poller 的 EventLoop，只用于线程归属检查
    EventLoop* ownerLoop_;
};


