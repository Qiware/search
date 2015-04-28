#if !defined(__INVERT_H__)
#define __INVERT_H__

#include "log.h"
#include "comm.h"
#include "list.h"
#include "btree.h"

typedef enum
{
    INVT_OK                         /* 成功 */
    , INVT_ERR = ~0x7FFFFFFF        /* 错误 */
} invert_err_e;

/* 倒排索引 */
typedef struct
{
} invert_index_t;

/* 倒排对象 */
typedef struct
{
    int mod;                        /* 哈希数组大小 */
    log_cycle_t *log;               /* 日志对象 */
    btree_t **tree;                 /* B树 */

    /* 内存池 */
    void *pool;                     /* 内存池 */
    mem_alloc_cb_t alloc;           /* 申请内存 */
    mem_dealloc_cb_t dealloc;       /* 释放内存 */
} invert_cntx_t;

/* 对外接口 */
invert_cntx_t *invert_creat(int max, log_cycle_t *log);
int invert_insert(invert_cntx_t *ctx, const char *word, const char *doc);
int invert_query(invert_cntx_t *ctx, const char *word, list_t *list);
int invert_remove(invert_cntx_t *ctx, const char *word);
int invert_destroy(invert_cntx_t *ctx);

#endif /*__INVERT_H__*/
