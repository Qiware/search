/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: vector.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年04月09日 星期六 07时59分27秒 #
 ******************************************************************************/
#if !defined(__VECTOR_H__)
#define __VECTOR_H__

typedef struct
{
    int incr;       /* 增量 */
    int cap;        /* 最大容量 */
    int len;        /* 实际长度 */
    void **arr;     /* 指针数组 */
} vector_t;

vector_t *vector_creat(int cap, int incr);
int vector_append(vector_t *vec, void *addr);
void *vector_find(vector_t *vec, find_cb_t find, void *args);
void *vector_get(vector_t *vec, int idx);
int vector_index(vector_t *vec, void *addr);
void *vector_del_by_idx(vector_t *vec, int idx);
int vector_delete(vector_t *vec, void *addr);
#define vector_len(vec) ((vec)->len)
#define vector_cap(vec) ((vec)->cap)
#define vector_incr(vec) ((vec)->incr)
int vector_destroy(vector_t *vec, mem_dealloc_cb_t dealloc, void *pool);

#endif /*__VECTOR_H__*/
