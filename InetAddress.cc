#include "InetAddress.h"

#include <strings.h> // bezero()
#include <string.h>  // strlen()

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    //将端口号转为网络字节序
    addr_.sin_port = htons(port);
    //将"127.0.0.1"点分十进制字符串转为32为IP 地址
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());

}

// 将 IP 地址转换为字符串格式，例如 "127.0.0.1"
std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    // 将二进制 IP 地址转为可读字符串形式
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}


// 将 IP 和端口号组合为字符串形式，例如 "127.0.0.1:8080"
std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    //ip:port
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    //从网络字节序转换回主机字节序
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end, ":%u", port);
    return buf;

}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}


// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIp() << std::endl;
//     std::cout << addr.toPort() << std::endl;
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }