#pragma once

#include "Poller.h"
#include "TimeStamp.h"

#include <vector>
#include <sys/epoll.h>


/**
 * epoll 的使用
 * epoll create
 * epoll_ctl add/mod/del
 * epoll_wait
 */

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    TimeStamp poll(int timeoutMS, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;

    //
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};