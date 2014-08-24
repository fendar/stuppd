#ifndef __SS_CONFIG_H__
#define __SS_CONFIG_H__ 1

#include "ss_core.h"

typedef int             ss_int_t;
typedef unsigned int    ss_uint_t;
typedef unsigned char   ss_uchar_t;

typedef struct ss_map_s ss_map_t;

struct ss_map_s{
    char        *key;
    char        *value;
    ss_map_t    *next;   
};

#define LF      (char)'\n'
#define CR      (char)'\r'
#define CRLF    "\r\n" 


#endif
