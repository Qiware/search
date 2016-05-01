#if !defined(__SDSD_H__)
#define __SDSD_H__

#include "ticket_lock.h"

#define SDSD_POOL_PAGE_NUM   (4)    /* 缓存池页数: 页越多锁冲突越小 但消耗更多的内存 */

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

#define SDSD_MOD_WR        (0)              /* 权限:写 */
#define SDSD_MOD_RD        (1)              /* 权限:读 */
    int mode;                               /* 当前权限(只能由发送线程修改) */
    int num;                                /* 数据块数 */
    time_t send_tm;                         /* 上次数据发送时间 */
} sdsd_pool_page_t;

typedef struct
{
    size_t size;                            /* 空间总长 */
    sdsd_pool_page_t page[SDSD_POOL_PAGE_NUM]; /* 缓存页对象 */
} sdsd_pool_head_t;

typedef struct
{
    void *addr[SDSD_POOL_PAGE_NUM];         /* 各页首地址 */
    sdsd_pool_head_t *head;                 /* 共享内存头部数据 */
} sdsd_pool_t;

sdsd_pool_t *sdsd_pool_creat(const char *fpath, int max, int _size);
sdsd_pool_t *sdsd_pool_attach(const char *fpath);
int sdsd_pool_push(sdsd_pool_t *pool, int type, int nid, const void *data, size_t len);
sdsd_pool_page_t *sdsd_pool_switch(sdsd_pool_t *pool);

#define sdsd_pool_destroy(pool)

#endif /*__SDSD_H__*/
