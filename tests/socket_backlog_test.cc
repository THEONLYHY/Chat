#include "Socket.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0)
    {
        return 1;
    }

    Socket socket(fd);
    socket.setListenBacklog(2048);
    if (socket.listenBacklog() != 2048)
    {
        return 2;
    }

    socket.setListenBacklog(0);
    if (socket.listenBacklog() != 2048)
    {
        return 3;
    }

    return 0;
}
