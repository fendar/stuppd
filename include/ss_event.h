#ifndef __SS_EVENT_H__
#define __SS_EVENT_H__

#include "ss_core.h"
#include "ss_mem.h"
#include "ss_socket.h"
#include <sys/epoll.h>      //epoll lib

//event
#define SS_EPOLL_SIZE       20
#define SS_MAX_CONNECTIONS  100
#define SS_NEVENTS           20
#define SS_EPOLL_WAIT_TIME  500         //0.5 seconds
#define SS_EVENT_TIMEOUT    60          //60 sec timeout

//http
#define SS_MAX_REQUEST_LINE 512        //max request line byte
#define SS_MAX_HEADER       2048        //max header byte

#define SS_HTTP_OK          200
#define SS_HTTP_FORBIDDEN   403
#define SS_HTTP_NOT_FOUND   404
#define SS_HTTP_BAD_REQUEST 400

#define SS_HTTP_ERROR       -1
#define SS_HTTP_AGAIN       1
#define SS_HTTP_DONE        0

#define SS_KP_ALIVE         1
#define SS_KP_ALIVE_TIMEOUT 10       //timeout seconds


typedef struct epoll_event          ss_epoll_event_t;
typedef struct ss_event_s           ss_event_t;
typedef struct ss_http_request_s    ss_http_request_t;
typedef struct ss_http_timer_s      ss_http_timer_t;

typedef void    (*handle_event_pt)(ss_event_t *);

struct ss_event_s {
    ss_sockfd_t         fd;
    handle_event_pt     handler;
    //void                *data;//there only for ss_http_request_t *
    ss_http_request_t   *request;
    ss_time_t           stime;
    ss_log_t            *log;
};

struct ss_http_request_s {
    //pool to alloc
    ss_pool_t   *pool;
    
    //listen socket
    ss_listen_t *listen;
    //request data
    ss_uint_t   method;//1 for GET, 2 for POST, 3 for OTHER
    ss_uint_t   http_version;//1 for 1.0;2 for 2.0; 3 for OTHER
    ss_int_t    kp_alive;   //0 for close,1 for keep-alive
    ss_str_t    uri;
    ss_str_t    args;
    ss_str_t    host;

    ss_log_t    *log;    
};

struct ss_http_timer_s {
    ss_event_t  *ev;
    ss_event_t  *next;
}

extern void ss_event_cycle(ss_listen_t *listen, ss_log_t *errlog);

#endif
