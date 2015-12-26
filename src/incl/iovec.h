/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: iovec.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2015年12月26日 星期六 08时11分09秒 #
 ******************************************************************************/

#if !defined(__IO_VEC_H__)
#define __IO_VEC_H__

#include "comm.h"

#define WIOV_MAX_NUM        (1024)      /* 最大发送个数 */

/* IOV原始数据信息 */
typedef struct
{
    void *addr;                         /* 起始地址 */
    size_t len;                         /* 原始长度 */
    size_t off;                         /* 发送偏移 */

    void *pool;                         /* 所属内存池 */
    mem_dealloc_cb_t dealloc;           /* 内存释放回调 */
} wiov_orig_t;

/* IOV对象(写) */
typedef struct
{
    int iov_cnt;                        /* 发送缓存个数 */
    int iov_idx;                        /* 当前正在发送的缓存索引 */
    wiov_orig_t orig[WIOV_MAX_NUM];     /* 原始信息(注: 与iov[]一一对应) */
    struct iovec iov[WIOV_MAX_NUM];     /* 发送缓存 */
} wiov_t;

#define wiov_isempty(wiov) (0 == (wiov)->iov_cnt) // 缓存已空
#define wiov_is_full(wiov) (WIOV_MAX_NUM == (wiov)->iov_cnt) // 缓存已满
#define wiov_left_space(wiov) (WIOV_MAX_NUM - (wiov)->iov_cnt)

#define wiov_item_begin(wiov) ((wiov)->iov + (wiov)->iov_idx)
#define wiov_item_num(wiov) ((wiov)->iov_cnt - (wiov)->iov_idx)

#define wiov_item_add(wiov, _addr, _len, _pool, _dealloc) /* 添加发送内容 */\
{ \
    (wiov)->iov[(wiov)->iov_cnt].iov_len = (_len); \
    (wiov)->iov[(wiov)->iov_cnt].iov_base = (char *)(_addr); \
    \
    (wiov)->orig[(wiov)->iov_cnt].off = 0; \
    (wiov)->orig[(wiov)->iov_cnt].len = (_len); \
    (wiov)->orig[(wiov)->iov_cnt].addr = (char *)(_addr); \
    \
    (wiov)->orig[(wiov)->iov_cnt].pool = (void *)(_pool); \
    (wiov)->orig[(wiov)->iov_cnt].dealloc = (mem_dealloc_cb_t)_dealloc; \
    \
    ++(wiov)->iov_cnt; \
}

#define wiov_item_reset(wiov, idx) /* 重置item项 */\
{ \
    (wiov)->iov[idx].iov_len = 0; \
    (wiov)->iov[idx].iov_base = NULL; \
    \
    (wiov)->orig[idx].off = 0; \
    (wiov)->orig[idx].len = 0; \
    (wiov)->orig[idx].addr = NULL; \
    \
    (wiov)->orig[idx].pool = NULL; \
    (wiov)->orig[idx].dealloc = NULL; \
    \
}

void wiov_item_clear(wiov_t *wiov);
int wiov_item_adjust(wiov_t *wiov, size_t n);

#endif /*__IO_VEC_H__*/
