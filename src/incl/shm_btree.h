#if !defined(__SHM_BTREE_H__)
#define __SHM_BTREE_H__

#include "log.h"
#include "comm.h"
#include "redo.h"
#include "shm_slab.h"

/* SHM-B树结点 */
typedef struct _shm_btree_node_t
{
    int num;                                        /* 关键字数 */
    off_t key;                                      /* 关键字(int *) */
    off_t data;                                     /* 结点承载(void **) */
    off_t child;                                    /* 孩子结点(shm_btree_node_t **) */
    off_t parent;                                   /* 父亲结点(shm_btree_node_t *) */
} shm_btree_node_t;

/* SHM-B树结构 */
typedef struct
{
    int max;                                        /* 关键字最大个数 */
    int min;                                        /* 最小关键字个数 */
    int sep_idx;                                    /* 结点分化的分割索引 */
    off_t root;                                     /* 根结点(shm_btree_node_t *) */

    size_t total;                                   /* 总大小 */
} shm_btree_t;

typedef struct
{
    int fd;                                         /* 描述符 */
    void *addr;                                     /* 首地址 */
    log_cycle_t *log;                               /* 日志对象 */

    shm_btree_t *btree;                             /* B树对象 */
    shm_slab_pool_t *pool;                          /* 内存池 */
} shm_btree_cntx_t;

extern shm_btree_cntx_t *shm_btree_creat(const char *path, int m, size_t total, log_cycle_t *log);
int shm_btree_insert(shm_btree_cntx_t *ctx, int key, void *data);
extern int shm_btree_remove(shm_btree_cntx_t *ctx, int key, void **data);
void *shm_btree_query(shm_btree_cntx_t *ctx, int key);
extern int shm_btree_destroy(shm_btree_cntx_t *ctx);
void shm_btree_print(shm_btree_cntx_t *ctx);

#endif /*__SHM_BTREE_H__*/
