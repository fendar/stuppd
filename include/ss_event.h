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

#define SS_HTTP_OK          2//200
#define SS_HTTP_FORBIDDEN   4//403
#define SS_HTTP_NOT_FOUND   8//404
//#define SS_HTTP_BAD_REQUEST 400
#define SS_HTTP_DENY        16//503
#define SS_HTTP_SERVER_ERROR 32//500
#define SS_HTTP_CLIENT_ERROR 64//400
#define SS_HTTP_CLOSE_FD    128//600
#define SS_HTTP_TIMEOUT     256//700

#define SS_HTTP_ERROR_STATUS 108//403 + 404 + 500 + 400
#define SS_HTTP_CLOSE_STATUS 384//600 + 700

#define SS_HTTP_ERROR       -1
#define SS_HTTP_AGAIN       1
#define SS_HTTP_DONE        0
#define SS_HTTP_CLOSE       2

#define SS_HTTP_DEFAULT_PAGE "index.html"
#define SS_HTTP_OUT_BUFFER_SIZE 1024

#define SS_KP_ALIVE         1
#define SS_KP_ALIVE_TIMEOUT 10       //timeout seconds

#define ss_buf_init(buf, len) (buf)->last = (buf)->pos = (buf)->start; (buf)->end = (buf)->start + (len)

typedef struct epoll_event          ss_epoll_event_t;
typedef struct ss_event_s           ss_event_t;
typedef struct ss_http_request_s    ss_http_request_t;
typedef struct ss_http_timer_s      ss_http_timer_t;
typedef struct ss_buf_s             ss_buf_t;
typedef struct ss_out_s             ss_out_t;

typedef void    (*handle_event_pt)(ss_event_t *);

struct ss_buf_s {
    char *start;
    char *last;
    char *pos;
    char *end;
};

struct ss_out_s {
    ss_buf_t    header;
    ss_int_t    send_file;
    ss_buf_t    body;
    ss_file_t   file;
};

struct ss_event_s {
    ss_sockfd_t         fd;
    handle_event_pt     handler;
    //void                *data;//there only for ss_http_request_t *
    ss_http_request_t   *request;
    ss_uint_t           flag;//status for finalize the request
    ss_time_t           stime;
    ss_log_t            *log;
};

struct ss_http_request_s {
    //pool to alloc
    ss_pool_t   *pool;
    
    ss_buf_t    recv_buf;
    //listen socket
    ss_listen_t *listen;
    //request data
    ss_uint_t   method;//1 for GET, 2 for POST, 3 for OTHER
    ss_uint_t   http_version;//1 for 1.0;2 for 1.1; 3 for OTHER
    ss_int_t    kp_alive;   //0 for close,1 for keep-alive
    ss_str_t    uri;
    ss_str_t    args;
    ss_str_t    host;
    char        *open_uri;//add later
    ss_int_t    res_type;//respons_type 0:text/html 1:text/css 2 text/js 3 image......

    ss_out_t    *out;
    ss_log_t    *log;    
};

struct ss_http_timer_s {
    ss_event_t      *ev;
    ss_http_timer_t *next;
};


extern void ss_event_cycle(ss_listen_t *listen, ss_log_t *errlog);

#endif
