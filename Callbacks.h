#pragma once


#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class TimeStamp;

// 一个指向TcpConnection的智能指针
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

using ConnectionCallback = std::function<void (const TcpConnectionPtr&)>;
using CloseCallback = std::function<void (const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void (const TcpConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void (const TcpConnectionPtr&, size_t)>;

using MessageCallback = std::function<void (const TcpConnectionPtr&,
                                            Buffer*,
                                            TimeStamp)>;

