#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <errno.h>

//防止一个线程创建多个EvetnLoop  thead_local
__thread EventLoop* t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeupfd 用于notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exist in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }

    //std::bind 可以忽略多余参数，因此可以将无参成员函数适配为接受参数的回调接口。所以省略了TimeStamp.
    //但 lambda 是强类型的，必须显式匹配函数签名，所以需要写成 [this](TimeStamp) 的形式。
    
    //wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->setReadCallback([this](TimeStamp){
        handleRead();
    });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping", this);

    while(!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller->poll(kPollTimeMs, &activeChannels_);

        //Poller监听哪些Channel发生事件，然后上报EventLoop，EventLoop负责通知Channel处理相应的事件
        for(Channel * channel : activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }

        //执行当前EventLoop事件循环需要执行的回调操作
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping", this);
    looping_ = false;
}

// 退出事件循环
// 1. loop在自己线程中调用 ，设置quit_ = true
// 2. 在其他线程中调用，发送wakeupFd_，唤醒loop所在线程，执行loop中的wakeupChannel_->handleEvent(pollReturnTime_);
void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread())
    {
        wakeup();
    }
}


void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前线程中，直接执行回调
    {
        cb();
    }
    else // 在其他线程中，将回调加入到队列中，唤醒loop所在线程执行回调
    {
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }
    /**
     1.别的线程投递：必须唤醒，否则 loop 可能睡到超时。
     2.正在执行 pending functors 时又有新任务入队：也唤醒，避免新任务被拖到很久以后才处理。
    */
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

// 唤醒loop所在线程，执行回调
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %zd bytes instead of 8", n);
    }
}

/// 更新某个 Channel 在 Poller 中关注的事件
void EventLoop::updateChannel(Channel * channel)
{
    poller->updateChannel(channel);
}

/// 从 Poller 中移除某个 Channel
void EventLoop::removeChannel(Channel * channel)
{
    poller->removeChannel(channel);
}

/// 当前 Poller 是否持有该 Channel
bool EventLoop::hasChannel(Channel * channel)
{
    return poller->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8", n);
    }
}

/// 执行 pendingFunctors_ 中的所有回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;

}