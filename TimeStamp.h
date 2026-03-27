#pragma once

#include <iostream>
#include <string>

class TimeStamp
{
public:
    TimeStamp();
    //explicit 需要对象不是隐式转换
    explicit TimeStamp(int64_t microSecondsSinceEpoch);
    static TimeStamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};