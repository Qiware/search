#if !defined(__INVERT_H__)
#define __INVERT_H__

#include "log.h"
#include "str.h"
#include "comm.h"
#include "btree.h"
#include "avl_tree.h"

typedef enum
{
    INVT_OK                         /* 成功 */
    , INVT_ERR = ~0x7FFFFFFF        /* 错误 */
} invert_err_e;

/* 文档对象 */
typedef struct
{
    str_t url;                      /* 文档路径 */
    int freq;                       /* 单词次数 */
    int off;                        /* 单词位置 */
} invt_doc_t;

/* 单词项 */
typedef struct
{
    str_t word;                     /* 单词 */

    btree_t *doc_list;              /* 文档列表(记录了该单词的所有文档的文档列表
                                       及单词在该文档出现的位置信息, etc. */
} invt_word_item_t;

/* 倒排对象 */
typedef struct
{
    int mod;                        /* 哈希数组大小 */
    log_cycle_t *log;               /* 日志对象 */
    avl_tree_t **dic;               /* 单词词典 */

    /* 内存池 */
    void *pool;                     /* 内存池 */
    mem_alloc_cb_t alloc;           /* 申请内存 */
    mem_dealloc_cb_t dealloc;       /* 释放内存 */
} invt_cntx_t;

/* 对外接口 */
invt_cntx_t *invert_creat(int max, log_cycle_t *log);
int invert_insert(invt_cntx_t *ctx, char *word, invt_doc_t *doc);
invt_word_item_t *invert_query(invt_cntx_t *ctx, char *word);
int invert_remove(invt_cntx_t *ctx, char *word, invt_word_item_t **item);
int invert_destroy(invt_cntx_t *ctx);

#endif /*__INVERT_H__*/
