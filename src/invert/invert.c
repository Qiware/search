/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invert.c
 ** 版本号: 1.0
 ** 描  述: 倒排索引、倒排文件处理
 **         如: 创建、插入、查找、删除、归并等
 ** 作  者: # Qifeng.zou # 2015.01.29 #
 ******************************************************************************/
#include "hash.h"
#include "invert.h"
#include "syscall.h"

/******************************************************************************
 **函数名称: invert_creat
 **功    能: 创建倒排对象
 **输入参数: 
 **     max: 数组长度
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 倒排对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
invert_cntx_t *invert_creat(int max, log_cycle_t *log)
{
    int idx;
    invert_cntx_t *ctx;
    btree_option_t option;

    /* > 创建对象 */
    ctx = (invert_cntx_t *)calloc(1, sizeof(invert_cntx_t));
    if (NULL == ctx)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    ctx->mod = max;
    ctx->log = log;
    ctx->pool = (void *)NULL;
    ctx->alloc = mem_alloc;
    ctx->dealloc = mem_dealloc;

    /* > 创建B树对象 */
    ctx->tree = (btree_t **)calloc(max, sizeof(btree_t *));
    if (NULL == ctx->tree)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(ctx);
        return NULL;
    }

    for (idx=0; idx<max; ++idx)
    {
        memset(&option, 0, sizeof(option));

        option.pool = (void *)NULL;
        option.alloc = mem_alloc;
        option.dealloc = mem_dealloc;

        ctx->tree[idx] = (btree_t *)btree_creat(3, &option);
        if (NULL == ctx->tree[idx])
        {
            log_error(log, "Create btree failed! idx:%d", idx);
            return NULL;
        }
    }

    return ctx;
}

/******************************************************************************
 **函数名称: invert_insert
 **功    能: 插入倒排信息
 **输入参数: 
 **     key: 关键字
 **     doc: 包含关键字的文档
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_insert(invert_cntx_t *ctx, const char *word, const char *doc)
{
    int idx;
    unsigned int key;

    key = hash_time33(word);
    idx = key % ctx->mod;

    if (btree_insert(ctx->tree[idx], key))
    {
        log_error(ctx->log, "Insert red-black-tree failed! word:%s doc:%s key:%lu idx:%d", word, doc, key, idx);
        return INVT_ERR;
    }
    
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_query
 **功    能: 查询倒排信息
 **输入参数: 
 **     word: 关键字
 **     list: 文档列表
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_query(invert_cntx_t *ctx, const char *word, list_t *list)
{
    int idx;
    void *addr;
    unsigned int key;

    key = hash_time33(word);
    idx = key % ctx->mod;

    addr = btree_query(ctx->tree[idx], key);
    if (NULL == addr)
    {
        log_error(ctx->log, "Query word [%s] failed! key:%d idx:%d", word, key, idx);
        return INVT_ERR;
    }
    
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_remove
 **功    能: 删除倒排信息
 **输入参数: 
 **     word: 关键字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_remove(invert_cntx_t *ctx, const char *word)
{
    int idx;
    unsigned int key;

    key = hash_time33(word);
    idx = key % ctx->mod;

    if (btree_remove(ctx->tree[idx], key))
    {
        log_error(ctx->log, "Query word [%s] failed! key:%d idx:%d", word, key, idx);
        return INVT_ERR;
    }
    
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_destroy
 **功    能: 销毁倒排对象
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_destroy(invert_cntx_t *ctx)
{
    int idx;

    for (idx=0; idx<ctx->mod; ++idx)
    {
        btree_destroy(ctx->tree[idx]);
    }

    FREE(ctx->tree);
    FREE(ctx);

    return 0;
}
