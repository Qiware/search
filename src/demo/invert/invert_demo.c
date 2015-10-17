/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invert_demo.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Mon 11 May 2015 02:41:30 PM CST #
 ******************************************************************************/

#include "invtab.h"

int main(void)
{
    int idx;
    invt_tab_t *tab;
    log_cycle_t *log;
    list_node_t *node;
    invt_word_doc_t *doc;
    invt_dic_word_t *word;


    log = log_init(LOG_LEVEL_DEBUG, "invert.log");

    /* > 创建倒排表 */
    tab = invtab_creat(1024, log);
    if (NULL == tab)
    {
        return -1;
    }

#define INVERT_INSERT(tab, word, url, freq) \
    if (invtab_insert(tab, word, url, freq)) \
    { \
        return -1; \
    }

    INVERT_INSERT(tab, "CSDN", "www.csdn.net", 5);
    INVERT_INSERT(tab, "BAIDU", "www.baidu.com", 5);
    INVERT_INSERT(tab, "BAIDU", "www.baidu2.com", 4);
    INVERT_INSERT(tab, "BAIDU", "www.baidu3.com", 2);
    INVERT_INSERT(tab, "BAIDU", "www.baidu4.com", 3);
    INVERT_INSERT(tab, "BAIDU", "www.baidu5.com", 10);
    INVERT_INSERT(tab, "凤凰网", "www.ifeng.com", 10);
    INVERT_INSERT(tab, "QQ", "www.qq.com", 10);
    INVERT_INSERT(tab, "SINA", "www.sina.com", 6);
    INVERT_INSERT(tab, "搜狐", "www.sohu.com", 7);

    /* > 搜索倒排表 */
    word = invtab_query(tab, "BAIDU");
    if (NULL == word
        || NULL == word->doc_list)
    {
        log_debug(log, "Didn't find anything!");
        return 0;
    }

    log_debug(log, "BAIDU: ");

    /* > 打印搜索结果 */
    idx = 0;
    node = word->doc_list->head;
    for (;NULL!=node; node=node->next)
    {
        doc = (invt_word_doc_t *)node->data;

        log_debug(log, "\t%d| url:%s freq:%d", ++idx, doc->url.str, doc->freq);
    }

    return 0;
}
