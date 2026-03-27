#pragma once

#include <arpa/inet.h> //提供 inet_addr, inet_ntop, htons, ntohs 等网络字节序函数
#include <netinet/in.h> 
#include <string>


class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
        {}
        //将IP 转为字符串形式("127.0.0.1")
        std::string toIp() const;
        //将IP 和端口号一起转为字符串 ("127.0.0.1:8080")
        std::string toIpPort() const;
        //返回主机字节序的端口号
        uint16_t toPort() const;

        const sockaddr_in* getSockAddr() const {return &addr_;}
        void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }
private:  
    sockaddr_in addr_;
};