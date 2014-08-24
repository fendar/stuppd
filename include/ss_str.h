#ifndef __SS_STR_H__
#define __SS_STR_H__ 1

#include "ss_core.h"
#include "ss_config.h"

typedef struct s_str_s {
    char *data;
    ss_uint_t len;
}ss_str_t;

#define ss_null_string              {null, 0}

#endif
