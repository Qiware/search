#if !defined(__CRWL_SCHED_H__)
#define __CRWL_SCHED_H__

#include <sys/select.h>
#include <hiredis/hiredis.h>

#define CRWL_SCHED_THD_NUM  (1)             /* SCHED线程数 */

/* 调度器对象 */
typedef struct
{
    int id;                                 /* 线程索引 */
    redisContext *redis;                    /* Redis对象 */
} crwl_sched_t;

void *crwl_sched_routine(void *_ctx);

#endif /*__CRWL_SCHED_H__*/
