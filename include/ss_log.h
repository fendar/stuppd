#ifndef __SS_LOG_H__
#define __SS_LOG_H__ 1

#include "ss_core.h"
#include "ss_file.h"

#define SS_LOG_MAX_LEN      1024

//data structure
typedef struct ss_log_s {
    ss_file_t *file;
}ss_log_t;


void ss_write_log(ss_log_t *log, const char *fmt, ...);
void ss_stderr_log(const char *fmt, ...);

#endif
