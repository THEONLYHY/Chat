#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <string>
#include <functional>

using std::cout;
using std::endl;
using std::string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
/// 
/// 1. 组合TcpServer对象
/// 2. 创建EventLoop 事件循环对象的指针
/// 3. 明确TcpServer构造函数需要什么参数, 输出ChatServer的构造函数
/// 4. ChatServer中注册处理连接的回调函数和处理读写事件的回调函数
/// 5.设置服务端合适的线程数量, muduo库会自己分配I/O线程和worker线程
class ChatServer
{
public:
    ChatServer(muduo::net::EventLoop *loop, //事件循环
               const muduo::net::InetAddress &listenAddr, // IP + Port
               const std::string &nameArg)  //服务器的名字
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        // 给服务器注册用户连接的创建和断开回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
        
        // 给服务器注册用户的读写事件
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        //设置服务器端的线程数量 1个I/O 线程 3个worker线程
        _server.setThreadNum(4);
    }

    //开启事件循环
    void start(){
        _server.start();
    }
private:
    // 专门处理用户的连接创建和断开 epoll listenfd accept
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if(conn->connected()){
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state: online " << endl;
        }
        else{
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state:offline " << endl;
            conn->shutdown();
            // _loop-quit();
        }
        
    }

    // 专门处理用户的读写事件
    void onMessage(const muduo::net::TcpConnectionPtr &conn, // 连接
                   muduo::net::Buffer *buff,                  // 缓冲区
                   muduo::Timestamp time)                    // 接收数据的时间信息
    {
        string reBuf = buff->retrieveAllAsString();
        cout << "recv data: " << reBuf << " time: " << time.toString() << endl;
        conn->send(reBuf);
     }
    muduo::net::TcpServer _server; // #1
    muduo::net::EventLoop *_loop;  // #2

};


// int main()
// {
//     muduo::net::EventLoop loop;
//     muduo::net::InetAddress addr("127.0.0.1", 6000);
//     ChatServer server(&loop, addr, "ChatServer");

//     server.start();
//     loop.loop();
//     return 0;
// }