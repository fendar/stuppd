#include "include/ss_core.h"
#include "include/ss_time.h"
#include <time.h>

size_t 
ss_format_time(char *t, const int maxsize)
{
    time_t tstamp = time(NULL);

    return strftime(t, maxsize, "%F %T ", localtime(&tstamp));
}
