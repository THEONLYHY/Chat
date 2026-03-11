#include "Thread.h"
#include "CurrentThread.h"

#include <memory>



Thread(ThreadFunc func, const string & name )
: started(false)
, joined_(false)
, tid_(0)
, func_(std::move(func))
, name(name_)
{
    setDefaultName();
}


~Thread::Thread()
{
    if (started_ && !joined_)
    {
        thread_-> detach();
    }
}

void Thraed::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())  
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

void Thread::start()
{
    stared_ = true;
    thread = std::make_shared<std::thread> ([&](){
        tid_ = Curr
    })
}

void Thread::join()
{

}