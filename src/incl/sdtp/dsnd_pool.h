#if !defined(__H__)
#define __H__

#include "ticket_lock.h"

#define DSND_POOL_PAGE_NUM   (4)    /* 缓存池页数: 页越多锁冲突越小 但消耗更多的内存 */

/**
 *   |<------------------------------ 共享内存 ------------------------------>|
 *  
 *    -------------------------------------- ---------------------------------
 *   |head|     缓     存     空     间     |     缓     存     空     间     |
 *    -------------------------------------- ---------------------------------
 *   ^    ^             ^                   ^                           ^     ^
 *   |    |<--- off --->|                   |<---------- off ---------->|     |
 *   |    |<-------------- size ----------->|<-------------- size ----------->|
 *  addr buf[0]                            buf[1]
 */

typedef struct
{
    int idx;                                /* 页号 */
    ticketlock_t lock;                      /* 锁 */

    size_t begin;                           /* 开始偏移(从共享内存起始处计算) */
    size_t size;                            /* 缓存总长 */
    size_t end;                             /* 结束偏移 */

    size_t off;                             /* 已用长度 */

#define DSND_MOD_WR        (0)              /* 权限:写 */
#define DSND_MOD_RD        (1)              /* 权限:读 */
    int mode;                               /* 当前权限(只能由发送线程修改) */
    int num;                                /* 数据块数 */
    time_t send_tm;                         /* 上次数据发送时间 */
} dsnd_pool_page_t;

typedef struct
{
    size_t size;                            /* 空间总长 */
    dsnd_pool_page_t page[DSND_POOL_PAGE_NUM]; /* 缓存页对象 */
} dsnd_pool_head_t;

typedef struct
{
    void *addr[DSND_POOL_PAGE_NUM];         /* 各页首地址 */
    dsnd_pool_head_t *head;                 /* 共享内存头部数据 */
} dsnd_pool_t;

dsnd_pool_t *dsnd_pool_creat(const char *fpath, int max, int _size);
dsnd_pool_t *dsnd_pool_attach(const char *fpath);
int dsnd_pool_push(dsnd_pool_t *pool, int type, int devid, const void *data, size_t len);
dsnd_pool_page_t *dsnd_pool_switch(dsnd_pool_t *pool);

#define dsnd_pool_destroy(pool)

#endif /*__H__*/
