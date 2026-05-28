#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

enum ConnState
{
    CONNECTING,
    ESTABLISHED,
    CLOSED
};

struct Options
{
    std::string host = "127.0.0.1";
    uint16_t port = 6000;
    int connections = 1;
    int durationSec = 10;
    int rampRate = 1000;
    int threads = 1;
    int payloadBytes = 64;
    int sendIntervalMs = 1000;
    bool verifyEcho = false;
    std::string metricsPath;
};

struct Metrics
{
    long long attempted = 0;
    long long established = 0;
    long long failed = 0;
    long long active = 0;
    long long normalClosed = 0;
    long long unexpectedClosed = 0;
    long long echoOk = 0;
    long long echoMismatch = 0;
    long long sendErrors = 0;
    long long recvErrors = 0;
    long long bytesSent = 0;
    long long bytesReceived = 0;
    std::vector<long long> latenciesUs;
};

struct Conn
{
    int fd = -1;
    ConnState state = CONNECTING;
    int id = 0;
    long long seq = 0;
    std::string out;
    std::string in;
    std::string expected;
    std::chrono::steady_clock::time_point lastSend;
    std::chrono::steady_clock::time_point sentAt;
};

void printHelp()
{
    std::cout
        << "Usage: long_conn_client [options]\n"
        << "  --host <ip>\n"
        << "  --port <port>\n"
        << "  --connections <n>\n"
        << "  --duration-sec <sec>\n"
        << "  --ramp-rate <connections_per_sec>\n"
        << "  --threads <n>\n"
        << "  --payload-bytes <n>\n"
        << "  --send-interval-ms <ms>\n"
        << "  --verify-echo\n"
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
        else if (arg == "--connections")
        {
            const char *value = requireValue("--connections");
            if (!value) return false;
            options->connections = std::atoi(value);
        }
        else if (arg == "--duration-sec")
        {
            const char *value = requireValue("--duration-sec");
            if (!value) return false;
            options->durationSec = std::atoi(value);
        }
        else if (arg == "--ramp-rate")
        {
            const char *value = requireValue("--ramp-rate");
            if (!value) return false;
            options->rampRate = std::atoi(value);
        }
        else if (arg == "--threads")
        {
            const char *value = requireValue("--threads");
            if (!value) return false;
            options->threads = std::atoi(value);
        }
        else if (arg == "--payload-bytes")
        {
            const char *value = requireValue("--payload-bytes");
            if (!value) return false;
            options->payloadBytes = std::atoi(value);
        }
        else if (arg == "--send-interval-ms")
        {
            const char *value = requireValue("--send-interval-ms");
            if (!value) return false;
            options->sendIntervalMs = std::atoi(value);
        }
        else if (arg == "--verify-echo")
        {
            options->verifyEcho = true;
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

int makeNonblockingSocket()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    return fd;
}

void updateEvents(int epollfd, const Conn &conn, int op)
{
    epoll_event event;
    std::memset(&event, 0, sizeof event);
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    if (conn.state == CONNECTING || !conn.out.empty())
    {
        event.events |= EPOLLOUT;
    }
    event.data.u32 = static_cast<uint32_t>(conn.id);
    ::epoll_ctl(epollfd, op, conn.fd, &event);
}

std::string makePayload(int id, long long seq, int payloadBytes)
{
    std::ostringstream out;
    out << id << ":" << seq << ":";
    std::string payload = out.str();
    if (static_cast<int>(payload.size()) < payloadBytes)
    {
        payload.append(static_cast<size_t>(payloadBytes - payload.size()), 'x');
    }
    else if (static_cast<int>(payload.size()) > payloadBytes)
    {
        payload.resize(static_cast<size_t>(payloadBytes));
    }
    return payload;
}

void closeConn(int epollfd, Conn &conn, Metrics &metrics, bool normal)
{
    if (conn.state == CLOSED)
    {
        return;
    }
    ::epoll_ctl(epollfd, EPOLL_CTL_DEL, conn.fd, nullptr);
    ::close(conn.fd);
    conn.fd = -1;
    if (conn.state == ESTABLISHED)
    {
        --metrics.active;
        if (normal)
        {
            ++metrics.normalClosed;
        }
        else
        {
            ++metrics.unexpectedClosed;
        }
    }
    conn.state = CLOSED;
}

void writeMetrics(std::ostream &out, const Options &options, const Metrics &metrics)
{
    double disconnectRate = metrics.established == 0
        ? 0.0
        : (100.0 * static_cast<double>(metrics.unexpectedClosed) / static_cast<double>(metrics.established));
    out << "{"
        << "\"timestamp_ms\":" << nowMs()
        << ",\"connections_target\":" << options.connections
        << ",\"connections_attempted\":" << metrics.attempted
        << ",\"connections_established\":" << metrics.established
        << ",\"connections_failed\":" << metrics.failed
        << ",\"active_connections\":" << metrics.active
        << ",\"normal_closed\":" << metrics.normalClosed
        << ",\"unexpected_closed\":" << metrics.unexpectedClosed
        << ",\"disconnect_rate_percent\":" << disconnectRate
        << ",\"echo_ok\":" << metrics.echoOk
        << ",\"echo_mismatch\":" << metrics.echoMismatch
        << ",\"send_errors\":" << metrics.sendErrors
        << ",\"recv_errors\":" << metrics.recvErrors
        << ",\"bytes_sent\":" << metrics.bytesSent
        << ",\"bytes_received\":" << metrics.bytesReceived
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

    int epollfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0)
    {
        std::cerr << "epoll_create1 failed: " << errno << "\n";
        return 3;
    }

    std::ofstream metricsFile;
    if (!options.metricsPath.empty())
    {
        metricsFile.open(options.metricsPath.c_str(), std::ios::out | std::ios::trunc);
        if (!metricsFile)
        {
            std::cerr << "failed to open metrics file: " << options.metricsPath << "\n";
            return 4;
        }
    }
    std::ostream &metricsOut = metricsFile ? metricsFile : std::cout;

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options.port);
    if (::inet_pton(AF_INET, options.host.c_str(), &addr.sin_addr) != 1)
    {
        std::cerr << "invalid host: " << options.host << "\n";
        return 5;
    }

    std::vector<Conn> conns;
    conns.reserve(static_cast<size_t>(options.connections));
    Metrics metrics;
    std::vector<epoll_event> events(1024);

    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::seconds(options.durationSec);
    auto nextConnect = start;
    auto nextReport = start + std::chrono::seconds(1);
    int connectIntervalUs = options.rampRate > 0 ? 1000000 / options.rampRate : 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto now = std::chrono::steady_clock::now();
        while (metrics.attempted < options.connections
               && (connectIntervalUs == 0 || now >= nextConnect))
        {
            Conn conn;
            conn.id = static_cast<int>(conns.size());
            conn.fd = makeNonblockingSocket();
            if (conn.fd < 0)
            {
                ++metrics.failed;
                ++metrics.attempted;
                break;
            }

            int rc = ::connect(conn.fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
            if (rc == 0)
            {
                conn.state = ESTABLISHED;
                conn.lastSend = now - std::chrono::milliseconds(options.sendIntervalMs);
                ++metrics.established;
                ++metrics.active;
            }
            else if (errno == EINPROGRESS)
            {
                conn.state = CONNECTING;
            }
            else
            {
                ::close(conn.fd);
                ++metrics.failed;
                ++metrics.attempted;
                continue;
            }

            conns.push_back(conn);
            updateEvents(epollfd, conns.back(), EPOLL_CTL_ADD);
            ++metrics.attempted;
            if (connectIntervalUs > 0)
            {
                nextConnect += std::chrono::microseconds(connectIntervalUs);
            }
        }

        now = std::chrono::steady_clock::now();
        for (auto &conn : conns)
        {
            if (conn.state != ESTABLISHED || !conn.out.empty())
            {
                continue;
            }
            if (options.verifyEcho && !conn.expected.empty())
            {
                continue;
            }
            if (now - conn.lastSend >= std::chrono::milliseconds(options.sendIntervalMs))
            {
                std::string payload = makePayload(conn.id, conn.seq++, options.payloadBytes);
                conn.out = payload;
                if (options.verifyEcho)
                {
                    conn.expected = payload;
                    conn.sentAt = now;
                }
                conn.lastSend = now;
                updateEvents(epollfd, conn, EPOLL_CTL_MOD);
            }
        }

        int n = ::epoll_wait(epollfd, events.data(), static_cast<int>(events.size()), 50);
        if (n > static_cast<int>(events.size()) / 2)
        {
            events.resize(events.size() * 2);
        }

        for (int i = 0; i < n; ++i)
        {
            uint32_t id = events[static_cast<size_t>(i)].data.u32;
            if (id >= conns.size())
            {
                continue;
            }
            Conn &conn = conns[id];
            if (conn.state == CLOSED)
            {
                continue;
            }

            uint32_t revents = events[static_cast<size_t>(i)].events;
            if (conn.state == CONNECTING && (revents & EPOLLOUT))
            {
                int err = 0;
                socklen_t len = sizeof err;
                if (::getsockopt(conn.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
                {
                    ++metrics.failed;
                    closeConn(epollfd, conn, metrics, false);
                    continue;
                }
                conn.state = ESTABLISHED;
                conn.lastSend = std::chrono::steady_clock::now() - std::chrono::milliseconds(options.sendIntervalMs);
                ++metrics.established;
                ++metrics.active;
                updateEvents(epollfd, conn, EPOLL_CTL_MOD);
            }

            if ((revents & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) && conn.state == ESTABLISHED)
            {
                closeConn(epollfd, conn, metrics, false);
                continue;
            }

            if ((revents & EPOLLOUT) && conn.state == ESTABLISHED && !conn.out.empty())
            {
                while (!conn.out.empty())
                {
                    ssize_t sent = ::send(conn.fd, conn.out.data(), conn.out.size(), 0);
                    if (sent > 0)
                    {
                        metrics.bytesSent += sent;
                        conn.out.erase(0, static_cast<size_t>(sent));
                    }
                    else if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                    {
                        break;
                    }
                    else
                    {
                        ++metrics.sendErrors;
                        closeConn(epollfd, conn, metrics, false);
                        break;
                    }
                }
                if (conn.state != CLOSED)
                {
                    updateEvents(epollfd, conn, EPOLL_CTL_MOD);
                }
            }

            if ((revents & EPOLLIN) && conn.state == ESTABLISHED)
            {
                char buf[4096];
                while (true)
                {
                    ssize_t received = ::recv(conn.fd, buf, sizeof buf, 0);
                    if (received > 0)
                    {
                        metrics.bytesReceived += received;
                        if (options.verifyEcho)
                        {
                            conn.in.append(buf, static_cast<size_t>(received));
                            while (!conn.expected.empty() && conn.in.size() >= conn.expected.size())
                            {
                                std::string actual = conn.in.substr(0, conn.expected.size());
                                conn.in.erase(0, conn.expected.size());
                                if (actual == conn.expected)
                                {
                                    ++metrics.echoOk;
                                    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - conn.sentAt).count();
                                    metrics.latenciesUs.push_back(latency);
                                }
                                else
                                {
                                    ++metrics.echoMismatch;
                                }
                                conn.expected.clear();
                            }
                        }
                    }
                    else if (received == 0)
                    {
                        closeConn(epollfd, conn, metrics, false);
                        break;
                    }
                    else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        break;
                    }
                    else
                    {
                        ++metrics.recvErrors;
                        closeConn(epollfd, conn, metrics, false);
                        break;
                    }
                }
            }
        }

        if (std::chrono::steady_clock::now() >= nextReport)
        {
            writeMetrics(metricsOut, options, metrics);
            nextReport += std::chrono::seconds(1);
        }
    }

    for (auto &conn : conns)
    {
        closeConn(epollfd, conn, metrics, true);
    }
    ::close(epollfd);

    if (!metrics.latenciesUs.empty())
    {
        std::sort(metrics.latenciesUs.begin(), metrics.latenciesUs.end());
        auto percentile = [&](double p) {
            size_t index = static_cast<size_t>((metrics.latenciesUs.size() - 1) * p);
            return metrics.latenciesUs[index];
        };
        metricsOut << "{"
                   << "\"timestamp_ms\":" << nowMs()
                   << ",\"latency_p50_us\":" << percentile(0.50)
                   << ",\"latency_p90_us\":" << percentile(0.90)
                   << ",\"latency_p99_us\":" << percentile(0.99)
                   << ",\"latency_max_us\":" << metrics.latenciesUs.back()
                   << "}" << std::endl;
    }
    writeMetrics(metricsOut, options, metrics);
    return metrics.failed == 0 && metrics.echoMismatch == 0 ? 0 : 1;
}
