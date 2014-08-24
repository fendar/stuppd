#ifndef __SS_SOCKET_H__
#define __SS_SOCKET_H__

#include "ss_core.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BACKLOG 20

typedef struct sockaddr_in  ss_sockaddr_in_t;
typedef struct sockaddr     ss_sockaddr_t;
typedef int                 ss_sockfd_t;
typedef struct ss_listen_s {
    ss_sockfd_t         fd;
    ss_int_t            port;
    ss_sockaddr_in_t    sockaddr_in;
    ss_uint_t           sockaddr_len;
}ss_listen_t;

extern  ss_int_t   ss_listening_init(ss_listen_t *);

#endif

