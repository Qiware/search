#if !defined(__CRWL_SCHED_H__)
#define __CRWL_SCHED_H__

#include <sys/select.h>
#include <hiredis/hiredis.h>

/* 调度器对象 */
typedef struct
{
    redisContext *redis;                    /* Redis对象 */

    int cmd_sck_id;                         /* 命令套接字 */

    fd_set rdset;                           /* 可读集合 */
    fd_set wrset;                           /* 可写集合 */

    int last_idx;                           /* 最近放入Worker任务队列 */ 
} crwl_sched_t;

void *crwl_sched_routine(void *_ctx);

#endif /*__CRWL_SCHED_H__*/
