#ifndef __SS_TIME_H__
#define __SS_TIME_H__ 1

#include "ss_core.h"
#include <time.h>

#define SS_FORMAT_TIME_LEN 20

#define ss_time_t   time_t
typedef struct tm   ss_tm_t;


extern size_t ss_format_time(char *t, const int len);

#endif
