#include "include/ss_core.h"

//static global variable
static  ss_int_t    ep;
static  ss_uint_t   clinums;    //connections num;accept new connection->clinums--;destroy a connection->clinums++;
static  ss_epoll_event_t    eearr[SS_NEVENTS];

static ss_http_request_t    *ss_alloc_request(ss_uint_t psize);
static void                 ss_accept_conn(ss_event_t *); 
static void                 ss_recv(ss_event_t *);
static ss_int_t             ss_add_event(ss_event_t *, ss_uint_t);
static ss_int_t             ss_del_event(ss_event_t *);

static void                 ss_http_finalize_request(ss_event_t *,ss_int_t flag);//flag will note 500,200,or 404...

void
ss_event_cycle(ss_listen_t *listening, ss_log_t *errlog)
{
    ss_event_t  listenev;
    ss_int_t    evnums, i;
    ss_http_timer_t timer_head, *timer;
    ss_time_t   nowtime;
    //init the epoll
    if ((ep = epoll_create(SS_EPOLL_SIZE)) == -1) {
        ss_write_log(errlog, "epoll_create error");
        //just quit
        exit(-1);
    }


    clinums = SS_MAX_CONNECTIONS;
    //add the listenfd to the epoll
    listenev.fd = listening->fd;
    listenev.handler = ss_accept_conn;
    listenev.log   = errlog;
    if (ss_add_event(&listenev, EPOLLIN) == -1) {
        //just quit
        exit(-1);
    }
    //start to handle event cycle
    header_timer.ev = NULL;
    header_timer.next = NULL;
    while(1) {
        //first checkout if some events is timeout,the timer head not store the event
        nowtime = time(NULL);//the time() may return (time_t)-1,but not a problem
        timer = header_timer->next;
        while (timer) {
            if (nowtime - timer->ev->stime > SS_EVENT_TIMEOUT) {
                //del the timer and del the event

            }
        }

        evnums = epoll_wait(ep, eearr, SS_NEVENTS, SS_EPOLL_WAIT_TIME);
        for (i = 0; i < evnums; ++i) {
            ss_event_t  *ev;
            ev = (ss_event_t *)(eearr[i].data.ptr);
            ev->handler(ev);
        }   
    }
}

static ss_http_request_t*
ss_alloc_request(ss_uint_t psize)
{
    //alloc the pool and the http_request_t
    ss_http_request_t   *request;
    ss_pool_t           *pool;
    
    request = (ss_http_request_t *)malloc(sizeof(ss_http_request_t));
    if (request == NULL) {
        return NULL;
    }
    pool = ss_create_pool(psize);
    if (pool == NULL) {
        free(request);
        return NULL;
    }

    request->pool = pool;

    return request;
}

static ss_int_t
ss_add_event(ss_event_t *ev, ss_uint_t etype)
{
    ss_int_t            flags;
    ss_sockfd_t         fd;
    ss_epoll_event_t    ee;
    memset(&ee, (char)0, sizeof(ee));
    //set start time,i don`t cache the time.In fact,we should consider the delay when use system call
    ev->stime = time(NULL);
    //set fd nonblock
    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    //add the event to the epoll
    ee.events |= etype;
    ee.data.ptr = ev;
    fd = ev->fd;

    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ee) == -1) {
        ss_write_log(ev->log, "epoll_ctl add for fd:%d error;stderror:%s", fd, strerror(errno));
        return -1;
    }
    return 0;
}

static ss_int_t
ss_del_event(ss_event_t *ev)
{
    ss_epoll_event_t ee;
    
    ee.events = 0;
    ee.data.ptr = NULL;

    if (epoll_ctl(ep, EPOLL_CTL_DEL, ev->fd, &ee) == -1) {
        ss_write_log(ev->log, "ss_del_event() error for fd:%d", ev->fd);
        return -1;
    }
    return 0;
}

static void
ss_accept_conn(ss_event_t *ev)
{

    ss_sockfd_t         lfd, cfd;
    ss_sockaddr_in_t    addr_in;
    ss_int_t            addr_len;
    ss_epoll_event_t    ee;
    ss_http_request_t   *request;
    ss_event_t          *rev;

    lfd = ev->fd;
    if ((cfd = accept(lfd, &addr_in, &addr_len)) == -1) {
        ss_write_log(ev->log, "accept error");    
        return;
    }
    //if the clinums is 0,close the connection
    if (0 == clinums) {
        ss_write_log(ev->log, "clinums is 0 and deny connection");
        close(cfd);
        return;
    }
    //new a pool
    request = ss_alloc_request(SS_POOL_SIZE);
    if (request == NULL) {
        close(cfd);
        return;
    }
    rev = (ss_event_t *)ss_alloc_from_pool(request->pool, sizeof(ss_event_t));
    if (rev == NULL) {
        close(cfd);
        return;
    }
    ss_write_log(ev->log, "accept success");
    rev->fd = cfd;
    rev->handler = ss_recv;
    rev->request = request;
    rev->log     = request->log = ev->log;
    if (ss_add_event(rev, EPOLLIN) == -1) {
        ss_free_pool(request->pool);
        free(request);
        return;
    } 
}

static void
ss_recv(ss_event_t *ev)
{
//note that sockfd is nonblock,so we may not recv all the data by call once
    ss_http_request_t   *request;
    ss_sockfd_t         fd;

    request = ev->request;
    fd      = ev->fd;
    
    //parse the request line,if error,set the fd EPOLLOUT and reset the handler(call back)
    if (ss_parse_request_line(ev) == SS_HTTP_ERROR) {
        ss_finalize_request(ev);
        return;
    }
    //parse the body
    if (ss_parse_request_body(ev) == SS_HTTP_ERROR) {
        ss_finalize_request(ev);
        return;
    }
//==========test============//
    char buf[5000];
    int  ret;

    ret = recv(fd, buf, 5000, 0);
    
    if (ret < 0) {
        ss_write_log(ev->log, "ss_recv() error for fd:%d", fd);
        ss_del_event(ev);
        close(fd);
        ss_free_pool(request->pool);
        free(request);
        return;
    } else if (ret == 0) {
        //client closed the socket
        ss_del_event(ev);
        close(fd);
        ss_free_pool(request->pool);
        free(request);
        return; 
    } else {
        send(fd, "HTTP/1.1 200 OK\r\n", sizeof("HTTP/1.1 200 OK\r\n") - 1, 0);
        send(fd, "Content-type: text/html\r\n", sizeof("Content-type: text/html\r\n") -1, 0);
        int len = sizeof("<h1>stuppd test</h1>") -1;
        send(fd, "Content-length: 20\r\n", sizeof("Content-length: 20\r\n") -1, 0);
        send(fd, "\r\n", sizeof("\r\n")-1, 0);
        send(fd, "<h1>stuppd test</h1>", len, 0);
        close(fd);
    }
}
