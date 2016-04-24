/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: vector.c
 ** 版本号: 1.0
 ** 描  述:
 ** 作  者: # Qifeng.zou # 2016年04月09日 星期六 07时59分19秒 #
 ******************************************************************************/
#include "comm.h"
#include "vector.h"

/******************************************************************************
 **函数名称: vector_creat
 **功    能: 创建vector对象
 **输入参数:
 **     cap: 默认容量
 **     incr: VEC增量
 **输出参数: NONE
 **返    回: VEC对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 #
 ******************************************************************************/
vector_t *vector_creat(int cap, int incr)
{
    vector_t *vec;

    vec = (vector_t *)calloc(1, sizeof(vector_t));
    if (NULL == vec) {
        return NULL;
    }

    vec->len = 0;
    vec->cap = cap;
    vec->incr = incr;

    vec->arr = (void **)calloc(cap, sizeof(void *));
    if (NULL == vec->arr) {
        free(vec);
        return NULL;
    }

    return vec;
}

/******************************************************************************
 **函数名称: vector_append
 **功    能: 往VEC插入数据
 **输入参数:
 **     vec: VEC对象
 **     addr: 插入数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 #
 ******************************************************************************/
int vector_append(vector_t *vec, void *addr)
{
    void **ptr;

    if (vec->len == vec->cap) {
        ptr = vec->arr;
        vec->arr = (void **)realloc(vec->arr, vec->cap+vec->incr);
        if (ptr == vec->arr) {
            return -1;
        }
        vec->cap += vec->incr;
    }

    vec->arr[vec->len] = addr;
    ++vec->len;

    return 0;
}

/******************************************************************************
 **函数名称: vector_get
 **功    能: 往VEC插入数据
 **输入参数:
 **     vec: VEC对象
 **     addr: 插入数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 #
 ******************************************************************************/
void *vector_get(vector_t *vec, int idx)
{
    if (idx >= vec->len) {
        return NULL;
    }
    return vec->arr[idx];
}

/******************************************************************************
 **函数名称: vector_find
 **功    能: 查找数据
 **输入参数:
 **     vec: VEC对象
 **     find: 查找回调
 **     args: 附加参数
 **输出参数: NONE
 **返    回: 查找到的数据地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 #
 ******************************************************************************/
void *vector_find(vector_t *vec, find_cb_t find, void *args)
{
    int idx;

    for (idx=0; idx<vec->len; ++idx) {
        if (find(vec->arr[idx], args)) {
            return vec->arr[idx];
        }
    }
    return NULL;
}

/******************************************************************************
 **函数名称: vector_del_by_idx
 **功    能: 通过idx删除数据
 **输入参数:
 **     vec: VEC对象
 **     idx: 数据索引
 **输出参数: NONE
 **返    回: 被删除的数据地址
 **实现描述: 使用最后一个成员填补被删的成员 -- 提高效率
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 #
 ******************************************************************************/
void *vector_del_by_idx(vector_t *vec, int idx)
{
    void *addr;

    if ((idx < 0) || (idx >= vec->len)) {
        return NULL;
    }

    addr = vec->arr[idx];
    if (idx != (vec->len-1)) {
        vec->arr[idx] = vec->arr[vec->len - 1];
    }
    vec->arr[vec->len - 1] = NULL;
    --vec->len;

    return addr;
}

/******************************************************************************
 **函数名称: vector_delete
 **功    能: 删除指定数据
 **输入参数:
 **     vec: VEC对象
 **     addr: 需要被删的数据
 **输出参数: NONE
 **返    回: 0:Succ !0:Fail
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.25 00:05:55 #
 ******************************************************************************/
int vector_delete(vector_t *vec, void *addr)
{
    int idx;

    idx = vector_index(vec, addr);
    if (-1 == idx) {
        return -1; /* Not found */
    }

    vector_del_by_idx(vec, idx);

    return 0;
}

/******************************************************************************
 **函数名称: vector_index
 **功    能: 通过addr获取索引
 **输入参数:
 **     vec: VEC对象
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.09 11:54:03 #
 ******************************************************************************/
int vector_index(vector_t *vec, void *addr)
{
    int idx;

    for (idx=0; idx<vec->len; ++idx) {
        if (vec->arr[idx] == addr) {
            return idx;
        }
    }

    return -1;
}

/******************************************************************************
 **函数名称: vector_destroy
 **功    能: 销毁Vector
 **输入参数:
 **     vec: VEC对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.23 13:54:27 #
 ******************************************************************************/
int vector_destroy(vector_t *vec, mem_dealloc_cb_t dealloc, void *pool)
{
    int idx;

    for (idx=0; idx<vec->len; ++idx) {
        dealloc(pool, vec->arr[idx]);
    }

    free(vec->arr);
    free(vec);
    return 0;
}
