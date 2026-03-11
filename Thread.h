#pragma

#include "nocopybale.h"
#include <functional>


class Thread
{
public: 
    using ThreadFunc = std::function<void ()>;

    explicit Thread(ThreadFunc, const string & name = string());

    ~Thread();

    void start();
    void join();

    bool started() const { return stared_; }
    pit_t tid() const { return tid_; }
    const string &name () const { return name_; }
private:

    void setDefaultName();
    
    bool started_;
    bool joined_;
    std::shared_ptr<thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;

    static std::atomic_int numCreated_;
};