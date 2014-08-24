#ifndef __SS_LOG_C__
#define __SS_LOG_C__ 1

#include "include/ss_core.h"
#include "include/ss_log.h"
#include "include/ss_time.h"
#include <stdarg.h>
#include <errno.h>


void
ss_write_log(ss_log_t *log, const char *fmt, ...)
{
    // log format: SS_LOG:Y-m-d H:i:s information
    va_list args;
    size_t cnt, usedlen, tempn, timelen;
    char errstr[SS_LOG_MAX_LEN];
    char time[SS_FORMAT_TIME_LEN + 1];
    char *p;

    if (log->file->fd == -1)
        return;

    p = errstr;
    usedlen = sizeof("SS_LOG:") - 1;
    memcpy(p, "SS_LOG:", usedlen);
    p += usedlen;

    //add time to the str
    timelen = ss_format_time(time, SS_FORMAT_TIME_LEN + 1);
    if (timelen) {
        memcpy(p, time, timelen);
        p += timelen;
        usedlen += timelen;
    }
    
    va_start(args, fmt);
    cnt = vsnprintf(p, SS_LOG_MAX_LEN - usedlen, fmt, args); 
    va_end(args);
    
    //after, cnt is the length of the string being written;
    if (cnt == -1) {
        tempn = sprintf(p, "vsnprintf() is error %s", strerror(errno));
        cnt = tempn + usedlen;
    } else if (cnt == SS_LOG_MAX_LEN - usedlen) {
        errstr[SS_LOG_MAX_LEN - 1] = '\n';
        cnt = SS_LOG_MAX_LEN;
    } else {
        cnt += usedlen;
        errstr[cnt] = '\n';
        ++cnt;
    }
    
    if (log->file->flag & (SS_FILE_WRONLY | SS_FILE_RDWR)){
        ss_write_file(log->file->fd, errstr, cnt); 
    }
    
}

void
ss_stderr_log(const char *fmt, ...)
{
    va_list args;
    char buf[SS_LOG_MAX_LEN];
    ss_int_t cnt;

    va_start(args, fmt);
    cnt = vsnprintf(buf, SS_LOG_MAX_LEN, fmt, args);
    va_end(args);
    
    write(STDERR_FILENO, buf, cnt);
}

#endif
