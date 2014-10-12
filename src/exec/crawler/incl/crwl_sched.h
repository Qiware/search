#if !defined(__CRWL_SCHED_H__)
#define __CRWL_SCHED_H__

#include <hiredis/hiredis.h>

/* 调度器对象信息 */
typedef struct
{
    redisContext *redis_ctx;                /* REDIS对象 */
} crwl_sched_t;

void *crwl_sched_routine(void *_ctx);

#endif /*__CRWL_SCHED_H__*/
