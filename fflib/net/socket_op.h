#ifndef _SOCKET_OP_H_
#define _SOCKET_OP_H_

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netinet/tcp.h>
#include <string>
using namespace std;

namespace ff {

struct socket_op_t
{
    static int set_nonblock(int fd_)
    {
        int flags;
        flags = fcntl(fd_, F_GETFL, 0);
        if ((flags = fcntl(fd_, F_SETFL, flags | O_NONBLOCK)) < 0)
        {
            return -1;
        }
    
        return 0;
    }
    static string getpeername(int sockfd)
    {
        string ret;
        struct sockaddr_in sa;
        ::socklen_t len = sizeof(sa);
        if(!::getpeername(sockfd, (struct sockaddr *)&sa, &len))
        {
            ret = ::inet_ntoa(sa.sin_addr);
        }
        return ret;
    }

    static int set_no_delay(int sockfd, bool flag_ = true)
    {
        int on = flag_? 1: 0;
        return ::setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,&on,sizeof(on));
    }

};

}

#endif
