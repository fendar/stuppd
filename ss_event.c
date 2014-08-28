#include "include/ss_core.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

//static global variable
static  ss_int_t            ep;
static  ss_uint_t           clinums;    //connections num;accept new connection->clinums--;destroy a connection->clinums++;
static  ss_epoll_event_t    eearr[SS_NEVENTS];
static  ss_http_timer_t     header_timer;
static  char                *ss_response_txt[] = {
                                "",
                                "",
                                "200 OK",
                                "403 Forbidden",
                                "404 Not Found",
                                "500 Internal Server Error",
                                "400 Bad Request"
};
static char                 *ss_response_type[] = {
                                "text/html; charset=utf-8",
                                "text/css; charset=utf-8",
                                "text/js; charset=utf-8",
                                "image/jpeg",
                                "image/png",
                                "image/gif"
};

//event
static ss_http_request_t    *ss_alloc_request(ss_uint_t psize);
static void                 ss_accept_conn(ss_event_t *); 
static void                 ss_recv(ss_event_t *);
static ss_int_t             ss_add_event(ss_event_t *, ss_uint_t);
static ss_int_t             ss_del_event(ss_event_t *);
static ss_int_t             ss_add_timer(ss_event_t *);
static ss_int_t             ss_del_timer(ss_event_t *);

static void                 ss_buf_memcpy(ss_buf_t *,const char *, ss_uint_t);
static ss_int_t             ss_file_to_buf(ss_int_t, ss_buf_t *);

//http
static ss_int_t             ss_http_parse_request_line(ss_event_t *);
static ss_int_t             ss_http_parse_request_header(ss_event_t *);
static void                 ss_http_finalize_request(ss_event_t *);//flag will note 500,200,or 404...
static ss_int_t             ss_check_request(ss_event_t *);
static ss_int_t             ss_http_prepare_send_data(ss_event_t *);
static void                 ss_http_send_data(ss_event_t *);
static ss_int_t             ss_http_init_out_file(ss_http_request_t *, char *);
static ss_int_t             ss_http_out_add_data(ss_http_request_t *,ss_uint_t);
static ss_int_t             ss_http_send_buf(ss_buf_t*, ss_int_t, ss_log_t *);


void
ss_event_cycle(ss_listen_t *listening, ss_log_t *errlog)
{
    ss_event_t          listenev;
    ss_int_t            evnums, i;
    ss_http_timer_t     *timer;
    ss_time_t           nowtime;

    //init the epoll
    if ((ep = epoll_create(SS_EPOLL_SIZE)) == -1) {
        ss_write_log(errlog, "epoll_create error");
        //just quit
        exit(-1);
    }


    clinums = SS_MAX_CONNECTIONS;
    header_timer.ev = NULL;
    header_timer.next = NULL;
    //add the listenfd to the epoll
    listenev.fd = listening->fd;
    listenev.handler = ss_accept_conn;
    listenev.log   = errlog;
    if (ss_add_event(&listenev, EPOLLIN) == -1) {
        ss_write_log(errlog, "ss_add_event() for listen event error:%s", strerror(errno));
        //just quit
        exit(-1);
    }
    //start to handle event cycle
    while(1) {
        //first checkout if some events is timeout,the timer head not store the event
        nowtime = time(NULL);//the time() may return (time_t)-1,but not a problem
        timer = header_timer.next;
        while (timer) {
            if (nowtime - timer->ev->stime > SS_EVENT_TIMEOUT) {
                //del the timer and del the event
                timer->ev->flag = SS_HTTP_TIMEOUT;
                ss_http_finalize_request(timer->ev);
            }
            timer = timer->next;
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
ss_mod_event(ss_event_t *ev, ss_uint_t etype)
{
    ss_epoll_event_t    ee;

    memset(&ee, (char)0, sizeof(ee));

    ee.events |= etype;
    ee.data.ptr = ev;
    
    if (epoll_ctl(ep, EPOLL_CTL_MOD, ev->fd, &ee) == -1) {
        ss_write_log(ev->log, "ss_mod_event() error:%s", strerror(errno));
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
    if ((cfd = accept(lfd, NULL, NULL)) == -1) {
        ss_write_log(ev->log, "accept error:%s fd:%d", strerror(errno), lfd);    
        return;
    }
    
//for more simple,i don`t handle the the error in a right way,i just close the fd
    if (0 == clinums) {
        //return 503
        ss_write_log(ev->log, "clinums is 0 and deny connection");
        close(cfd);
        return;
    }
    //new a pool
    request = ss_alloc_request(SS_POOL_SIZE);
    if (request == NULL) {
        ss_write_log(ev->log, "ss_alloc_request() error:%s", strerror(errno));
        close(cfd);
        return;
    }
    rev = (ss_event_t *)ss_alloc_from_pool(request->pool, sizeof(ss_event_t));
    if (rev == NULL) {
        ss_write_log(ev->log, "ss_alloc_request() error:%s", strerror(errno));
        close(cfd);
        return;
    }

    //init the recv buffer
    request->recv_buf.start = (char *)ss_alloc_from_pool(request->pool, SS_MAX_REQUEST_LINE + SS_MAX_HEADER);
    request->recv_buf.end   = (char *)(request->recv_buf.start + SS_MAX_REQUEST_LINE + SS_MAX_HEADER);
    request->recv_buf.last  = request->recv_buf.pos = request->recv_buf.start;

    rev->fd = cfd;
    rev->handler = ss_recv;
    rev->request = request;
    rev->log     = request->log = ev->log;
    rev->stime   = time(NULL);
    if (ss_add_event(rev, EPOLLIN) == -1 || ss_add_timer(rev) == -1) {
        rev->flag = SS_HTTP_CLOSE_FD;
        ss_http_finalize_request(rev);
    }
}

static void
ss_recv(ss_event_t *ev)
{
//note that sockfd is nonblock,so we may not recv all the data by call once
    ss_http_request_t   *request;
    ss_sockfd_t         fd;
    ss_int_t            status;

    request = ev->request;
    fd      = ev->fd;
    
    //update the time
    ev->stime = time(NULL);
    //parse the request line,if error,set the fd EPOLLOUT and reset the handler(call back)
//here is a little question:I don`t recv all the data before I parse the config.So when I find request_line or request_body,then I close the fd and do not recv data any more though there may be some data in buffer. 
    status = ss_http_parse_request_line(ev);
    if (status == SS_HTTP_ERROR) {
        ev->flag = SS_HTTP_CLIENT_ERROR;
        goto end_r;
    } else if (status == SS_HTTP_CLOSE){
        ev->flag = SS_HTTP_CLOSE_FD;
        goto end_r;
    } else if (status == SS_HTTP_AGAIN) {
        return;
    } 
    //parse the body
    status = ss_http_parse_request_header(ev);
    if (status == SS_HTTP_ERROR) {
        ev->flag = SS_HTTP_CLIENT_ERROR;
        goto end_r;
    } else if (status == SS_HTTP_CLOSE){
        ev->flag = SS_HTTP_CLOSE_FD;
        goto end_r;
    } else if (status == SS_HTTP_AGAIN) {
        return;
    }
    
    //now send data to the client
    //first check that client get is avalible
    ev->flag = ss_check_request(ev);
    ss_http_finalize_request(ev);    

end_r:
    ss_http_finalize_request(ev);
    return;
}

static ss_int_t
ss_http_parse_request_line(ss_event_t *ev)
{
//1, read data 2,parse data 3, return value
    ss_http_request_t   *request;
    ss_buf_t            *recv_buf;
    ss_int_t            rlen, i, idx, len, read_len, is_again;
    char                *data;

    request  = ev->request;
    recv_buf = &request->recv_buf;
    read_len = SS_MAX_REQUEST_LINE - (int)(recv_buf->last - recv_buf->start);
   
    if (read_len == 0) {
        //this is the same function as "is_again"
        ss_write_log(ev->log, "client data is too big for request line");
        return SS_HTTP_ERROR;
    }
    rlen = recv(ev->fd, recv_buf->last, read_len, 0);
    if (rlen == 0) {
        //client close the fd
        ss_write_log(ev->log, "client close the fd");
        return SS_HTTP_CLOSE;
    } else if (rlen == -1) {
        //if errno is EAGAIN OR EWOULDBLOCK,it notes that data in protocol is empty
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SS_HTTP_AGAIN;
        } else {
            ss_write_log(ev->log, "recv from client error in parse_request_line:%s", strerror(errno));
            return SS_HTTP_ERROR;
        }
    }
    data = recv_buf->pos;
    recv_buf->last += rlen;
    is_again = read_len - rlen > 0 ? 1 : 0;
    //parse the data,I only handle the method "GET".for not mastering "sscanf()" call,I do not use it

        //parse the method,only interested for "GET";i >= rlen,may read again if is_again is true
    idx = i = 0;
    rlen = recv_buf->last - recv_buf->pos;//now rlen mean all the data that read
    while (i < rlen && isalpha(data[i]))
        ++i;
    len = i - idx;
    if (i >= rlen && is_again)
        return SS_HTTP_AGAIN;
    if (i >= rlen || data[i] != ' ' || len != 3 || strncasecmp(&data[idx], "GET", 3) != 0) {
        ss_write_log(ev->log, "http method is not \"GET\"");
        return SS_HTTP_ERROR;
    }
    request->method = 1;
        
        //parse the url not including args
    idx = ++i;
    while(i < rlen && data[i] != '?' && data[i] != ' ' && data[i] != '\r')
        ++i;
    if (i >= rlen && is_again)
        return SS_HTTP_AGAIN;
    if (i >= rlen) {
        ss_write_log(ev->log, "the request line is too big");
        return SS_HTTP_ERROR;
    }
    request->uri.data = &data[idx];
    request->uri.len  = i - idx;
        
        //ignore the args
    while (i < rlen && data[i] != ' ')
         ++i;
    if (i >= rlen && is_again)
        return SS_HTTP_AGAIN;
    if (i >= rlen) {
        ss_write_log(ev->log, "the request line is too big 2");
        return SS_HTTP_ERROR;
    }
       
        //parse the protocol,only handle the HTTP/1.X,the reset should be HTTP/1.XCRLF
    idx = ++i;
    if (rlen - i < sizeof("HTTP/1.X\r\n") - 1 && is_again)
        return SS_HTTP_AGAIN;
    if (strncasecmp(&data[i], "HTTP/1.", sizeof("HTTP/1.") - 1) != 0) {
        ss_write_log(ev->log, "is not HTTP protocol");
        return SS_HTTP_ERROR;
    }
    i += sizeof("HTTP/1.") - 1;
    if (data[i] != '0' && data[i] != '1') {
        ss_write_log(ev->log, "is not HTTP 1.0 or 1.1, %c", data[i]);
        return SS_HTTP_ERROR;
    }
    request->http_version = (int)(data[i] - '0') + 1;
    ++i;

    if (data[i] != '\r' || data[i+1] != '\n') {
        ss_write_log(ev->log, "the end flag is error:%c%c", data[i], data[i+1]);
        return SS_HTTP_ERROR;
    }

    recv_buf->pos = &data[i+2]; 
    return SS_HTTP_DONE;
}

static ss_int_t
ss_http_parse_request_header(ss_event_t *ev)
{
//I Ignore any header,plz forgive me for more simple ^_^
    ss_http_request_t   *request;
    ss_buf_t            *recv_buf;
    ss_int_t            i, rlen, read_len;
    char                *data;

    request     = ev->request;
    recv_buf    = &request->recv_buf;
    data        = recv_buf->pos;
    
    //check out if needing recv,the following check is safe
    if (strncmp(recv_buf->last - 4, "\r\n\r\n", sizeof("\r\n\r\n") - 1) != 0) {
        read_len    = SS_MAX_HEADER - (int)(recv_buf->last - recv_buf->pos);
        if (read_len == 0) {
            ss_write_log(ev->log, "client data is too big for head");
            return SS_HTTP_ERROR;
        }
        rlen = recv(ev->fd, recv_buf->last, read_len, 0);
        if (rlen == 0) {
            ss_write_log(ev->log, "client close the fd");
            return SS_HTTP_CLOSE;
        } else if (rlen == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return SS_HTTP_AGAIN;
            }
            else {
                ss_write_log(ev->log,"recv from client error in parse_request_header:%s", strerror(errno));
                return SS_HTTP_ERROR;
            }   
        }
        recv_buf->last += rlen;
        //check out if recv "\r\n\r\n" which notes that do not need to recv data again
        //because we hanle GET,so the last four bytes are "\r\n\r\n" or return SS_HTTP_AGAIN
        if (strncmp(recv_buf->last - 4, "\r\n\r\n", sizeof("\r\n\r\n") - 1) != 0) {
            return SS_HTTP_AGAIN;
        }
    }
    
    //now recv all the data,start to parse,ignore header
    recv_buf->pos = recv_buf->last;

    return SS_HTTP_DONE;
}

static ss_int_t
ss_add_timer(ss_event_t *ev)
{
    ss_http_timer_t *timer, *newtimer;
    ss_pool_t       *pool;

    pool = ev->request->pool;
    timer = &header_timer;
    //we allocate memory from ev->request->pool,because the pool is destroyed,the timer is deleted
    newtimer = (ss_http_timer_t *)ss_alloc_from_pool(pool, sizeof(ss_http_timer_t));
    if (newtimer == NULL) {
        ss_write_log(ev->log, "ss_add_timer() error for newtimer:%s", strerror(errno));
        return -1;
    }
    newtimer->ev = ev;
    newtimer->next = NULL;
    //find the final timer
    while(timer->next)
        timer = timer->next;
    timer->next = newtimer;
    
    return 0;
}

static ss_int_t
ss_del_timer(ss_event_t *ev)
{
    ss_http_timer_t *timer, *pre_timer;

    pre_timer = &header_timer;
    timer = header_timer.next;
    
    while (timer && timer->ev != ev) {
        pre_timer = timer;
        timer = timer->next;
    }
    if (timer) {
        pre_timer->next = timer->next;
        return 0;
    }
    return -1;
}

static ss_int_t
ss_check_request(ss_event_t *ev)
{
//here just check the uri
    ss_http_request_t   *request;
    ss_str_t            *uri;
    char                *open_uri;
    ss_map_t            *mapp, maphead;
    ss_int_t            i;

    request = ev->request;
    uri     = &request->uri;
    
    maphead.next    = fileconf;
    mapp            = &maphead;
    
    //deny the request that uri includes "../"
    for (i = 0; i < (int)uri->len - 3; ++i) {
        if (uri->data[i] == '.' && uri->data[i + 1] == '.' && uri->data[i + 2] == '/') {
            ss_write_log(ev->log, "uri includes '../'");
            return SS_HTTP_CLIENT_ERROR;
        }
    }
    request->res_type = 0;
    //check out the out file type
    if (uri->len > 2 && uri->data[uri->len - 3] == '.' && uri->data[uri->len - 2] == 'j' && uri->data[uri->len - 1] == 's')
        request->res_type = 2;
    if (uri->len > 3 && uri->data[uri->len - 4] == '.') {
        if (uri->data[uri->len - 3] == 'j' && uri->data[uri->len - 2] == 'p' && uri->data[uri->len - 1] == 'g')
            request->res_type = 3;
        else if (uri->data[uri->len-3] == 'p' && uri->data[uri->len-2] == 'n' && uri->data[uri->len-1] == 'g')
            request->res_type = 4;
        else if (uri->data[uri->len-3] == 'g' && uri->data[uri->len-2] == 'i' && uri->data[uri->len-1] == 'f')
            request->res_type = 5;
    }
    
    open_uri = (char *)ss_alloc_from_pool(request->pool, wwwpath.len + uri->len + 1);
    if (open_uri == NULL) {
        ss_write_log(ev->log, "alloc for open_uri error:%s", strerror(errno));
        return SS_HTTP_SERVER_ERROR;
    }
    memcpy(open_uri, wwwpath.data, wwwpath.len);
    memcpy(open_uri + wwwpath.len, uri->data, uri->len);
    open_uri[wwwpath.len + uri->len + 1] = '\0';
    
    request->open_uri = open_uri;//add later

    //check out if the file extists and is available
    if (access(open_uri, F_OK) == -1) {
        ss_write_log(ev->log, "file not exists:%s", open_uri);
        return SS_HTTP_NOT_FOUND;
    }
    if (access(open_uri, R_OK) == -1) {
        ss_write_log(ev->log, "file not access to read:%s", open_uri);
        return SS_HTTP_FORBIDDEN;
    }
    return SS_HTTP_OK;
}

static void
ss_http_finalize_request(ss_event_t *ev)
{
    /*ss_write_log(ev->log, "finalize start,status:%d", ev->flag);*/
//free pool, free request
    if (ev == NULL)
        return;

    ss_http_request_t   *request;
    ss_uint_t           status;

// SS_HTTP_CLOSE_FD/SS_HTTP_TIMEOUT=========SS_HTTP_CLOSE_STATUS
//SS_HTTP_NOT_FOUND/SS_HTTP_FORBIDDEN/SS_HTTP_CLIENT_ERROR/SS_HTTP_SERVER_ERROR==============SS_HTTP_ERROR_STATUS
//SS_HTTP_OK
//are all the status should to be handled
    request = ev->request;
    status  = ev->flag;
    
    if (status & SS_HTTP_CLOSE_STATUS) {
        //if the ev is not in epoll or timer, that`s not a problem;
        ss_write_log(ev->log, "close on fd:%d because %d",ev->fd, status);
        ss_del_event(ev);
        ss_del_timer(ev);
        close(ev->fd);
        ss_free_pool(request->pool);
        free(request);
        return ;
    }
    
    request->out = NULL;
    if (ss_http_prepare_send_data(ev) == -1) {
        ev->flag = SS_HTTP_CLOSE_FD;
        ss_http_finalize_request(ev);
        return;
    }
    //send the data, set the event to the epoll
    ev->stime   = time(NULL);
    ev->handler = ss_http_send_data;
    if (ss_mod_event(ev, EPOLLOUT) == -1) {
        //just close
        ev->flag = SS_HTTP_CLOSE_FD;
        ss_http_finalize_request(ev);
        return;
    }
}

static void
ss_http_send_data(ss_event_t *ev)
{
    ss_http_request_t   *r;
    ss_int_t            wlen, status;
    ss_out_t            *out;
    ss_buf_t            *obuf;

    r       = ev->request;
    out     = r->out;
    obuf    = &out->header;

    if (obuf->pos == obuf->last) {
        out->send_file = 1;//now is send body
        obuf = &out->body;
    }

    status = ss_http_send_buf(obuf, ev->fd, ev->log);
    if (status == SS_HTTP_AGAIN) {
        return;
    } else if (status == SS_HTTP_ERROR) {
        ev->flag = SS_HTTP_CLOSE_FD;
        ss_http_finalize_request(ev);
        return;
    }
    if (out->send_file != 1)
        return;
    //SS_HTTP_DONE
    ev->flag = SS_HTTP_CLOSE_FD;
    ss_http_finalize_request(ev);
}

static ss_int_t
ss_http_send_buf(ss_buf_t *buf, ss_int_t fd, ss_log_t *log)
{
    ss_int_t slen, size;

    if (buf->pos == buf->last)
        return SS_HTTP_DONE;
    
    size = 1024 > buf->last - buf->pos ? buf->last - buf->pos : 1024;
    slen = send(fd, buf->pos, size, 0);
    if (slen == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SS_HTTP_AGAIN;
        }
        ss_write_log(log, "send data error:%s", strerror(errno));
        return SS_HTTP_ERROR;
    }
    buf->pos += slen;
    if (buf->pos == buf->last)
        return SS_HTTP_DONE;
    return SS_HTTP_AGAIN;
}

static ss_int_t
ss_http_prepare_send_data(ss_event_t *ev)
{
//I`m fucking thinking how to do it to be more close to the perfect for one day during this fucking good time because the school is open,and have a headache and I give up
//Ignore following errors
    ss_uint_t           flag;
    ss_http_request_t   *r;
    ss_pool_t           *pool;
    ss_map_t            *mapp;
    ss_out_t            *out;
    ss_file_t           *file;

    flag    = ev->flag;
    r       = ev->request;
    pool    = r->pool;
  //  out     = (ss_out_t *)ss_alloc_from_pool(pool, sizeof(ss_out_t));
    file    = &out->file;
    mapp    = fileconf;

    r->out = (ss_out_t *)ss_alloc_from_pool(pool, sizeof(ss_out_t));
    out = r->out;

    if (out == NULL) {
        ss_write_log(ev->log, "ss_alloc for out error:%s", strerror(errno));
        return -1;
    }
//SS_HTTP_NOT_FOUND/SS_HTTP_FORBIDDEN/SS_HTTP_CLIENT_ERROR/SS_HTTP_SERVER_ERROR==============SS_HTTP_ERROR_STATUS
//SS_HTTP_OK
//are the the status should to be handled
    if (flag & SS_HTTP_NOT_FOUND) {
        out->send_file = -1;//no file
        while (mapp) {
            if (mapp->key[2] == '4') {
                ss_http_init_out_file(r, mapp->value);
                break;
            }
            mapp = mapp->next;
        }
        //add data to the header buffer
        if (ss_http_out_add_data(r, SS_HTTP_NOT_FOUND) == -1)
            return -1;
    } else if (flag & SS_HTTP_FORBIDDEN) {
        out->send_file = -1;
        while (mapp) {
            if (mapp->key[2] == '3') {
                ss_http_init_out_file(r, mapp->value);
                break;
            }
            mapp = mapp->next;
        }
        if (ss_http_out_add_data(r, SS_HTTP_FORBIDDEN) == -1)
            return -1;
    } else if (flag & SS_HTTP_SERVER_ERROR) {
        out->send_file = -1;
        while (mapp) {
            if (mapp->key[2] == '0') {
                ss_http_init_out_file(r, mapp->value);
                break;
            }
            mapp = mapp->next;
        }
        if (ss_http_out_add_data(r, SS_HTTP_SERVER_ERROR) == -1)
            return -1;
    } else if (flag & SS_HTTP_OK) {
        out->send_file = -1;
        ss_http_init_out_file(r, r->open_uri);
        //if not regular file,return 403
        if (!S_ISREG(out->file.fileinfo->st_mode)) {
            ev->flag = SS_HTTP_FORBIDDEN;
            ss_http_finalize_request(ev);
        } 
        if (ss_http_out_add_data(r, SS_HTTP_OK) == -1)
           return -1; 
    } else if (flag & SS_HTTP_CLIENT_ERROR){
        out->send_file = -1;
        if (ss_http_out_add_data(r, SS_HTTP_CLIENT_ERROR) == -1)
            return -1;
    }
    return 0;
}

static ss_int_t
ss_http_out_add_data(ss_http_request_t *r, ss_uint_t code)
{
    ss_uint_t    bsize, clen, txt_idx, status;
    ss_out_t    *out;
    ss_buf_t    *headb, *bodyb;
    ss_file_t   *file;
    char        ctline[50];

    out     = r->out;
    headb   = &out->header;
    bodyb   = &out->body;
    status  = code;
    file    = &out->file;
    memset(ctline, (char)0, sizeof(ctline));

    headb->start = (char *)ss_alloc_from_pool(r->pool, SS_HTTP_OUT_BUFFER_SIZE);
    if (headb->start == NULL) {
        ss_write_log(r->log, "ss_alloc for head buffer error:%s", strerror(errno));
        return -1;
    }
    ss_buf_init(headb, SS_HTTP_OUT_BUFFER_SIZE);
    
    bsize = out->send_file == -1 ? SS_HTTP_OUT_BUFFER_SIZE / 2 : out->file.fileinfo->st_size;
    bodyb->start = (char *)ss_alloc_from_pool(r->pool, bsize);
    if (bodyb->start == NULL) {
        ss_write_log(r->log, "ss_alloc for body buffer error:%s", strerror(errno));
        return -1;
    }
    ss_buf_init(bodyb, bsize);

    txt_idx = 0;
    while (status) {
        ++txt_idx;
        status = status >> 1;
    }
    //caculate the content-length

    clen = out->send_file == -1 ? strlen(ss_response_txt[txt_idx]) : out->file.fileinfo->st_size;
    //add data to buffer, I do not check if the data is out of buffer
    //response line
    ss_buf_memcpy(headb, "HTTP/1.1 ", sizeof("HTTP/1.1"));
    ss_buf_memcpy(headb, ss_response_txt[txt_idx], strlen(ss_response_txt[txt_idx]));
    ss_buf_memcpy(headb, CRLF, sizeof(CRLF) - 1);
    sprintf(ctline, "Content-Length: %d"CRLF, clen);
    ss_buf_memcpy(headb, ctline, strlen(ctline));
    ss_buf_memcpy(headb, "Server: StuPPd"CRLF, sizeof("Server: StuPPd"CRLF) - 1);
    memset(ctline, 0, sizeof(ctline));
    sprintf(ctline, "Content-Type: %s"CRLF, ss_response_type[r->res_type]);
    ss_buf_memcpy(headb, ctline, strlen(ctline));
    ss_buf_memcpy(headb, CRLF, sizeof(CRLF) - 1);
    
    //add data to body
    if (out->send_file == -1) {
        ss_buf_memcpy(bodyb, ss_response_txt[txt_idx], clen);
    } else {
        if (ss_file_to_buf(file->fd, bodyb) == -1)
            return -1;
    }
    return 0;
}

static ss_int_t
ss_file_to_buf(ss_int_t fd, ss_buf_t *buf)
{
    ss_int_t    rlen;

    while (buf->last < buf->end) {
        rlen = ss_read_file(fd, buf->last, 1024);
        if (rlen == 0)
            return 0;
        if (rlen == -1)
            return -1;
        buf->last += rlen;
    }
}

static ss_int_t
ss_http_init_out_file(ss_http_request_t *r, char *filename)
{
    ss_out_t    *out;
    ss_pool_t   *pool;
    ss_file_t   *file;

    out     = r->out;
    pool    = r->pool;
    file    = &out->file;

    file->filename.data  = filename;
    file->filename.len   = strlen(filename);

    if ((file->fd = ss_open_file(filename, SS_FILE_RDONLY, SS_FILE_RDONLY, SS_FILE_ACCESS)) == -1) {
        ss_write_log(r->log,"open error file for 404 error:%s", strerror(errno));
        return -1;
    }
    file->flag = SS_FILE_RDONLY;
    file->fileinfo = (ss_fileinfo_t *)ss_alloc_from_pool(pool, sizeof(ss_fileinfo_t));
    if (file->fileinfo == NULL) {
        ss_write_log(r->log, "ss_alloc for fileinfo error in ss_http_init_out:%s", strerror(errno));
        close(file->fd);
        return -1;
    }
    if (stat(filename, file->fileinfo) == -1) {
        ss_write_log(r->log, "stat in ss_http_init_out error:%s", strerror(errno));
        close(file->fd);
        return -1;
    }
    out->send_file = 0;
    return 0;
}

static void
ss_buf_memcpy(ss_buf_t *buf, const char *s, ss_uint_t len)
{
    len = (buf->end - buf->last) < len ? (buf->end - buf->last) : len;
    
    memcpy(buf->last, s, len);
    buf->last += len;
}
