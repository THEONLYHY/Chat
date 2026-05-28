#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "TimeStamp.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

struct Options
{
    std::string host = "0.0.0.0";
    uint16_t port = 6000;
    int threads = 2;
    int backlog = 4096;
    bool tcpNoDelay = false;
    bool quiet = false;
    size_t highWaterMark = 64 * 1024 * 1024;
    size_t maxOutputBuffer = 0;
    int reportIntervalSec = 1;
    std::string metricsPath;
};

struct Metrics
{
    std::atomic<long long> active{0};
    std::atomic<long long> peak{0};
    std::atomic<long long> accepted{0};
    std::atomic<long long> closed{0};
    std::atomic<long long> messagesIn{0};
    std::atomic<long long> messagesOut{0};
    std::atomic<long long> bytesIn{0};
    std::atomic<long long> bytesOut{0};
    std::atomic<long long> highWaterEvents{0};
    std::atomic<long long> outputBufferPeak{0};

    void updatePeak(long long value)
    {
        long long old = peak.load();
        while (value > old && !peak.compare_exchange_weak(old, value))
        {
        }
    }

    void updateOutputPeak(size_t value)
    {
        long long candidate = static_cast<long long>(value);
        long long old = outputBufferPeak.load();
        while (candidate > old && !outputBufferPeak.compare_exchange_weak(old, candidate))
        {
        }
    }
};

void printHelp()
{
    std::cout
        << "Usage: echo_server_bench [options]\n"
        << "  --host <ip>\n"
        << "  --port <port>\n"
        << "  --threads <n>\n"
        << "  --backlog <n>\n"
        << "  --tcp-no-delay\n"
        << "  --high-water-mark <bytes>\n"
        << "  --max-output-buffer <bytes>\n"
        << "  --quiet\n"
        << "  --report-interval-sec <sec>\n"
        << "  --metrics-jsonl <path>\n";
}

bool parseArgs(int argc, char *argv[], Options *options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        auto requireValue = [&](const char *name) -> const char* {
            if (i + 1 >= argc)
            {
                std::cerr << name << " requires a value\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            printHelp();
            std::exit(0);
        }
        else if (arg == "--host")
        {
            const char *value = requireValue("--host");
            if (!value) return false;
            options->host = value;
        }
        else if (arg == "--port")
        {
            const char *value = requireValue("--port");
            if (!value) return false;
            options->port = static_cast<uint16_t>(std::atoi(value));
        }
        else if (arg == "--threads")
        {
            const char *value = requireValue("--threads");
            if (!value) return false;
            options->threads = std::atoi(value);
        }
        else if (arg == "--backlog")
        {
            const char *value = requireValue("--backlog");
            if (!value) return false;
            options->backlog = std::atoi(value);
        }
        else if (arg == "--tcp-no-delay")
        {
            options->tcpNoDelay = true;
        }
        else if (arg == "--high-water-mark")
        {
            const char *value = requireValue("--high-water-mark");
            if (!value) return false;
            options->highWaterMark = static_cast<size_t>(std::strtoull(value, nullptr, 10));
        }
        else if (arg == "--max-output-buffer")
        {
            const char *value = requireValue("--max-output-buffer");
            if (!value) return false;
            options->maxOutputBuffer = static_cast<size_t>(std::strtoull(value, nullptr, 10));
        }
        else if (arg == "--quiet")
        {
            options->quiet = true;
        }
        else if (arg == "--report-interval-sec")
        {
            const char *value = requireValue("--report-interval-sec");
            if (!value) return false;
            options->reportIntervalSec = std::atoi(value);
        }
        else if (arg == "--metrics-jsonl")
        {
            const char *value = requireValue("--metrics-jsonl");
            if (!value) return false;
            options->metricsPath = value;
        }
        else
        {
            std::cerr << "unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

long long nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void writeMetrics(std::ostream &out, const Options &options, const Metrics &metrics)
{
    out << "{"
        << "\"timestamp_ms\":" << nowMs()
        << ",\"io_threads\":" << options.threads
        << ",\"active_connections\":" << metrics.active.load()
        << ",\"peak_connections\":" << metrics.peak.load()
        << ",\"accepted_total\":" << metrics.accepted.load()
        << ",\"closed_total\":" << metrics.closed.load()
        << ",\"messages_in_total\":" << metrics.messagesIn.load()
        << ",\"messages_out_total\":" << metrics.messagesOut.load()
        << ",\"bytes_in_total\":" << metrics.bytesIn.load()
        << ",\"bytes_out_total\":" << metrics.bytesOut.load()
        << ",\"high_water_events\":" << metrics.highWaterEvents.load()
        << ",\"output_buffer_peak\":" << metrics.outputBufferPeak.load()
        << "}" << std::endl;
}

int main(int argc, char *argv[])
{
    Options options;
    if (!parseArgs(argc, argv, &options))
    {
        printHelp();
        return 2;
    }

    if (options.quiet)
    {
        Logger::instance().setQuiet(true);
    }

    Metrics metrics;
    std::ofstream metricsFile;
    if (!options.metricsPath.empty())
    {
        metricsFile.open(options.metricsPath.c_str(), std::ios::out | std::ios::trunc);
        if (!metricsFile)
        {
            std::cerr << "failed to open metrics file: " << options.metricsPath << "\n";
            return 3;
        }
    }

    EventLoop loop;
    InetAddress listenAddr(options.port, options.host);
    TcpServer server(&loop, listenAddr, "EchoBench");
    server.setThreadNum(options.threads);
    server.setListenBacklog(options.backlog);

    server.setConnectionCallback([&](const TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            conn->setTcpNoDelay(options.tcpNoDelay);
            conn->setMaxOutputBufferBytes(options.maxOutputBuffer);
            conn->setHighWaterMarkCallback([&](const TcpConnectionPtr &c, size_t bytes) {
                metrics.highWaterEvents.fetch_add(1);
                metrics.updateOutputPeak(bytes);
                if (options.maxOutputBuffer > 0 && bytes > options.maxOutputBuffer)
                {
                    c->forceClose();
                }
            }, options.highWaterMark);
            long long current = metrics.active.fetch_add(1) + 1;
            metrics.accepted.fetch_add(1);
            metrics.updatePeak(current);
        }
        else
        {
            metrics.closed.fetch_add(1);
            metrics.active.fetch_sub(1);
        }
    });

    server.setMessageCallback([&](const TcpConnectionPtr &conn, Buffer *buffer, TimeStamp) {
        std::string message = buffer->retrieveAllAsString();
        metrics.messagesIn.fetch_add(1);
        metrics.bytesIn.fetch_add(static_cast<long long>(message.size()));
        conn->send(message);
        metrics.messagesOut.fetch_add(1);
        metrics.bytesOut.fetch_add(static_cast<long long>(message.size()));
        metrics.updateOutputPeak(conn->outputBufferBytes());
    });

    std::atomic<bool> running(true);
    std::thread reporter([&]() {
        while (running.load())
        {
            std::this_thread::sleep_for(std::chrono::seconds(options.reportIntervalSec));
            if (metricsFile)
            {
                writeMetrics(metricsFile, options, metrics);
            }
            else if (!options.quiet)
            {
                writeMetrics(std::cout, options, metrics);
            }
        }
    });

    server.start();
    if (!options.quiet)
    {
        std::cout << "echo_server_bench listening on " << listenAddr.toIpPort()
                  << " threads=" << options.threads
                  << " backlog=" << options.backlog << std::endl;
    }
    loop.loop();
    running = false;
    reporter.join();
    return 0;
}
