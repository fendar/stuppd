#include "include/ss_core.h"
#include "include/ss_config.h"
#include "include/ss_mem.h"

ss_pool_t *
ss_create_pool(ss_uint_t size)
{
    ss_pool_t   *pool;
    void        *data;

    pool = (ss_pool_t *)malloc(sizeof(ss_pool_t));
    if (NULL == pool) {
        //error should be logged by the caller when the function is returned;
        //ss_write_log(&errlog, "malloc for ss_pool_t in ss_create_pool() error.strerror: %s", strerror(errno));
        return NULL;
    }
    data = malloc(size);
    if (NULL == data) {
        /*ss_write_log(&errlog, "malloc for data in ss_create_pool() error.strerror: %s", strerror(errno));*/
        free(pool); 
        return NULL;
    }

    pool->start = data;
    pool->now = data;
    pool->max = size;
    pool->next = NULL;
    
    return pool;
}

//the arg must be from ss_create_pool(ss_uint_t)
void
ss_free_pool(ss_pool_t *first)
{
    ss_pool_t   *p, *old;
    old = first;

    while(old) {
        p = old->next;
        free(old->start);
        free(old);
        old = p;
    }
}

void*
ss_alloc_from_pool(ss_pool_t *pool, ss_uint_t size)
{
    void        *data;
    ss_pool_t   *p, *pre, *newpool;
    ss_uint_t   newsize;

    p = pool;
    newsize = 0;
    if (!p) {
        /*ss_write_log(&errlog, "null poll in ss_alloc_from_pool");*/
        return NULL;
    }
    //if the current pool is big enough
    while (p) {
        if (size > p->max) {
            newsize += p->max;
            pre = p;
            p = p->next;
            continue;
        } else {
            p->max -= size;
            p->now += size;
            return (void *)(p->now - size);
        }
    }
    
    //alloc a new pool
    newsize = newsize > size ? 2 * newsize : 2 * size;
    newpool = ss_create_pool(newsize);
    if (NULL == newpool) {
        /*ss_write_log(&errlog, "alloc a new pool in ss_alloc_from pool error.strerror: %s", strerror(errno));*/
        return NULL;
    }
    pre->next = newpool;
    newpool->max -= size;
    newpool->now += size;
    return (void *)(newpool->now -size);
}
