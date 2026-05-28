
#include "Logger.h"
#include "TimeStamp.h"
//  获得日志唯一的实例对象
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}
// 设置日志的级别
void Logger::setLogLevel(int level)
{
    logLevel_= level;
}

void Logger::setQuiet(bool quiet)
{
    quiet_ = quiet;
}
//  写日志
void Logger::log(std::string msg)
{
    if (quiet_ && logLevel_ == INFO)
    {
        return;
    }

    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印时间和msg
    std::cout << TimeStamp::now().toString() << ":" << msg << std::endl;
}
