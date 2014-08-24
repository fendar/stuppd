#include "include/ss_core.h"
#include "include/ss_socket.h"

ss_int_t
ss_listen_init(ss_listen_t *listening)
{
    ss_sockaddr_in_t    *addr_inp;
    ss_uint_t           addr_len;
    ss_sockfd_t         lfd;
    int                 reuseaddr;

    addr_inp = &listening->sockaddr_in;
    addr_len = sizeof(ss_sockaddr_in_t);
    //init the sockaddr
    memset(addr_inp, 0, addr_len);
        //creat a socket
    if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
        //set sock reuse
    reuseaddr = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
        ss_stderr_log("setsockopt error\n");
        return -1;
    }
        //init sockaddr;
    addr_inp->sin_family = AF_INET;
    addr_inp->sin_port   = htons(listening->port);
    addr_inp->sin_addr.s_addr = htonl(INADDR_ANY);

        //bind
    if (bind(lfd, addr_inp, addr_len) == -1) {
        ss_stderr_log("bind error:%s\n", strerror(errno));
        return -1;
    }
        //listen
    if (listen(lfd, BACKLOG) == -1) {
        ss_stderr_log("listen error\n");
        return -1;
    }
    listening->fd = lfd;
    listening->sockaddr_len = addr_len;
    return 0;
}
