#ifndef __SS_FILE_H__
#define __SS_FILE_H__ 1

#include "ss_core.h"
#include "ss_config.h"
#include "ss_str.h"
#include <fcntl.h>


#define SS_FILE_RDWR            (O_RDWR)
#define SS_FILE_RDONLY          (O_RDONLY)
#define SS_FILE_WRONLY          (O_WRONLY)
#define SS_FILE_APPEND          (O_APPEND)
#define SS_FILE_OPEN_OR_CREAT   (O_CREAT)
#define SS_FILE_TRUNC           (O_TRUNC)
#define SS_FILE_NONBLOCK        (O_NONBLOCK)
#define SS_FILE_ACCESS          0640

#define ss_open_file(name, flag, mode, access)                  \
        open((const char *)name, flag | mode, access)
#define ss_close_file           close
#define ss_read_file            read
#define ss_write_file           write

#define ss_dup                  dup
#define ss_dup2                 dup2

typedef struct stat             ss_fileinfo_t;

typedef struct ss_file_s {
    ss_int_t        fd;//if the file is not opened,the fd will be -1
    ss_str_t        filename;
    ss_fileinfo_t   *fileinfo; 
    ss_uint_t       flag;
}ss_file_t;

#endif
