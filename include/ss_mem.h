#ifndef __SS_MEM_H__
#define __SS_MEM_H__ 1

#include "ss_core.h"

#define SS_ALLOC_SIZE   1024
#define SS_POOL_SIZE    4096        //start to alloc 4KB for every request

typedef struct ss_pool_s    ss_pool_t;

struct ss_pool_s {
    void        *start;
    void        *now;
    ss_uint_t   max;
    ss_pool_t   *next;
};

extern  ss_pool_t   *ss_create_pool(ss_uint_t size);
extern  void        ss_free_pool(ss_pool_t *first);
extern  void        *ss_alloc_from_pool(ss_pool_t *pool, ss_uint_t size);

#endif
