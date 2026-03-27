#pragma once

#include "noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <sys/types.h> // pid_t

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

private:
    void setDefaultName();

    bool started_{false};
    bool joined_{false};
    std::shared_ptr<std::thread> thread_;
    pid_t tid_{0};
    ThreadFunc func_;
    std::string name_;

    static std::atomic_int numCreated_;
};