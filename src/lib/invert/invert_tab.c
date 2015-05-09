/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: invert.c
 ** 版本号: 1.0
 ** 描  述: 倒排索引、倒排文件处理
 **         如: 创建、插入、查找、删除、归并等
 **     1. 单词词典: 使用平衡二叉树组织(原因: 查询会比修改删除更加频繁)
 **     2. 文档列表: 使用B树组织
 ** 作  者: # Qifeng.zou # 2015.04.29 #
 ******************************************************************************/
#include "hash.h"
#include "invert_tab.h"
#include "syscall.h"

/******************************************************************************
 **函数名称: invert_tab_dic_word_cmp
 **功    能: 比较单词的大小
 **输入参数: 
 **     word: 单词
 **     data: 与word进行比较的单词对应的数据
 **输出参数:
 **返    回: KEY值
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
static int invert_tab_dic_word_cmp(char *word, void *data)
{
    invt_dic_word_t *dw = (invt_dic_word_t *)data;

    return strcmp(word, dw->word.str);
}

/******************************************************************************
 **函数名称: invert_tab_creat
 **功    能: 创建倒排表
 **输入参数: 
 **     max: 数组长度
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 倒排对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
invt_tab_t *invert_tab_creat(int max, log_cycle_t *log)
{
    int idx;
    invt_tab_t *tab;
    avl_opt_t opt;

    /* > 创建对象 */
    tab = (invt_tab_t *)calloc(1, sizeof(invt_tab_t));
    if (NULL == tab)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    tab->mod = max;
    tab->log = log;
    tab->pool = (void *)NULL;
    tab->alloc = mem_alloc;
    tab->dealloc = mem_dealloc;

    /* > 创建单词词典 */
    tab->dic = (avl_tree_t **)tab->alloc(tab->pool, max * sizeof(avl_tree_t *));
    if (NULL == tab->dic)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        FREE(tab);
        return NULL;
    }

    for (idx=0; idx<max; ++idx)
    {
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)NULL;
        opt.alloc = mem_alloc;
        opt.dealloc = mem_dealloc;

        tab->dic[idx] = avl_creat(&opt,
                            (key_cb_t)hash_time33_ex,
                            (avl_cmp_cb_t)invert_tab_dic_word_cmp);
        if (NULL == tab->dic[idx])
        {
            log_error(log, "Create avl-tree failed! idx:%d", idx);
            invert_tab_destroy(tab);
            return NULL;
        }
    }

    return tab;
}

/******************************************************************************
 **函数名称: invt_word_add
 **功    能: 新建单词
 **输入参数: 
 **     tab: 全局对象
 **     word: 单词
 **     len: 单词长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.29 #
 ******************************************************************************/
static invt_dic_word_t *invt_word_add(invt_tab_t *tab, char *word, int len)
{
    int idx;
    list_opt_t opt;
    invt_dic_word_t *dw;

    idx = hash_time33(word) % tab->mod;

    /* > 创建数据对象 */
    dw = tab->alloc(tab->pool, sizeof(invt_dic_word_t));
    if (NULL == dw)
    {
        log_error(tab->log, "Alloc memory failed!");
        return NULL;
    }

    memset(dw, 0, sizeof(invt_dic_word_t));

    do
    {
        /* > 设置word标签 */
        dw->word.str = tab->alloc(tab->pool, len + 1);
        if (NULL == dw->word.str)
        {
            log_error(tab->log, "Alloc memory failed!");
            break;
        }

        snprintf(dw->word.str, len + 1, "%s", word);
        dw->word.len = strlen(word);

        /* > 创建文档列表 */
        opt.pool = (void *)NULL;
        opt.alloc = mem_alloc;
        opt.dealloc = mem_dealloc;

        dw->doc_list = (list_t *)list_creat(&opt);
        if (NULL == dw->doc_list)
        {
            log_error(tab->log, "Create btree failed! word:%s", word);
            break;
        }

        /* > 插入单词词典 */
        if (avl_insert(tab->dic[idx], word, len, (void *)dw))
        {
            log_error(tab->log, "Insert avl failed! word:%s idx:%d", word, idx);
            break;
        }

        return dw;
    } while(0);

    if (dw->doc_list) { tab->dealloc(tab->pool, dw->doc_list); }
    if (dw->word.str) { tab->dealloc(tab->pool, dw->word.str); }
    if (dw) { tab->dealloc(tab->pool, dw); }

    return NULL;
}

/******************************************************************************
 **函数名称: invt_word_add_doc
 **功    能: 添加文档列表
 **输入参数: 
 **     tab: 全局对象
 **     word: 单词
 **     len: 单词长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建文档项
 **     2. 将文档项加入文档列表
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.29 #
 ******************************************************************************/
static int invt_word_add_doc(invt_tab_t *tab, invt_dic_word_t *dw, const char *url, int freq)
{
    int len;
    invt_word_doc_t *doc;

    /* > 创建文档项 */
    doc = tab->alloc(tab->pool, sizeof(invt_word_doc_t));
    if (NULL == doc)
    {
        log_error(tab->log, "Alloc memory failed!");
        return INVT_ERR;
    }

    len = strlen(url);
    doc->url.str = tab->alloc(tab->pool, len + 1);
    if (NULL == doc->url.str)
    {
        log_error(tab->log, "Alloc memory failed!");
        tab->dealloc(tab->pool, doc);
        return INVT_ERR;
    }

    snprintf(doc->url.str, len+1, "%s", url);
    doc->url.len = len;
    doc->freq = freq;

    /* > 插入文档列表 */
    if (list_lpush(dw->doc_list, doc))
    {
        log_error(tab->log, "Push into list failed! word:%s url:%s", dw->word.str, url);
        tab->dealloc(tab->pool, doc->url.str);
        tab->dealloc(tab->pool, doc);
        return INVT_ERR;
    }

    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_tab_insert
 **功    能: 插入倒排信息
 **输入参数: 
 **     tab: 全局对象
 **     word: 关键字
 **     url: 包含关键字的文档
 **     freq: 词频
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_tab_insert(invt_tab_t *tab, char *word, const char *url, int freq)
{
    int idx;
    avl_node_t *node;
    invt_dic_word_t *dw;

    idx = hash_time33(word) % tab->mod;

    /* > 查找单词项 */
    node = avl_query(tab->dic[idx], word, strlen(word));
    if (NULL == node)
    {
        dw = invt_word_add(tab, word, strlen(word));
        if (NULL == dw)
        {
            log_error(tab->log, "Create word dw failed!");
            return INVT_ERR;
        }
    }
    else
    {
        dw = (invt_dic_word_t *)node->data;
    }

    /* > 插入文档列表 */
    if (invt_word_add_doc(tab, dw, url, freq))
    {
        log_error(tab->log, "Add document dw failed!");
        return INVT_ERR;
    }
   
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_tab_query
 **功    能: 查询倒排信息
 **输入参数: 
 **     tab: 全局对象
 **     word: 关键字
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
invt_dic_word_t *invert_tab_query(invt_tab_t *tab, char *word)
{
    int idx;
    avl_node_t *node;

    idx = hash_time33(word) % tab->mod;

    node = avl_query(tab->dic[idx], word, strlen(word));
    if (NULL == node)
    {
        log_error(tab->log, "Query word [%s] failed! idx:%d", word, idx);
        return NULL;
    }
    
    return (invt_dic_word_t *)node->data;
}

/******************************************************************************
 **函数名称: invert_tab_remove
 **功    能: 删除倒排信息
 **输入参数: 
 **     tab: 全局对象
 **     word: 关键字
 **输出参数:
 **     dw: 单词项数据
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_tab_remove(invt_tab_t *tab, char *word)
{
    int idx;
    invt_dic_word_t *dw;

    idx = hash_time33(word) % tab->mod;

    if (avl_delete(tab->dic[idx], word, strlen(word), (void **)&dw))
    {
        log_error(tab->log, "Query word [%s] failed! idx:%d", word, idx);
        return INVT_ERR;
    }
    
    list_destroy(dw->doc_list, tab->pool, (mem_dealloc_cb_t)mem_dealloc);
    tab->dealloc(tab->pool, dw->word.str);
    tab->dealloc(tab->pool, dw);
    
    return INVT_OK;
}

/******************************************************************************
 **函数名称: invert_tab_destroy
 **功    能: 销毁倒排对象
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.28 #
 ******************************************************************************/
int invert_tab_destroy(invt_tab_t *tab)
{
    int idx;

    for (idx=0; idx<tab->mod; ++idx)
    {
        avl_destroy(tab->dic[idx]);
    }

    tab->dealloc(tab->pool, tab->dic);
    FREE(tab);

    return 0;
}
