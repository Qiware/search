/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: html_tree.c
 ** 版本号: 1.0
 ** 描  述: HTML的处理
 **         这此文件中主要包含了HTML处理的对外接口
 ** 作  者: # Qifeng.zou # 2013.02.18 #
 ******************************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "html_tree.h"
#include "html_comm.h"
#include "common.h"
#include "log.h"


/* 是否为根路径 */
#define HtmlIsRootPath(path) (0 == strcmp(path, "."))

/* 是否为绝对路径 */
#define HtmlIsAbsPath(path) ('.' == path[0])

static html_node_t *_html_delete_empty(html_tree_t *html, Stack_t *stack, html_node_t *node);

/******************************************************************************
 **函数名称: html_creat_empty
 **功    能: 创建空HTML树
 **输入参数: NONE
 **输出参数: NONE
 **返    回: HTML树
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
html_tree_t *html_creat_empty(void)
{
    html_tree_t *html = NULL;

    /* 1. Create tree */
    html = (html_tree_t*)calloc(1, sizeof(html_tree_t));
    if (NULL == html)
    {
        log2_error("Calloc failed!");
        return NULL;
    }

    /* 2. Add root node */
    html->root = html_node_creat(HTML_NODE_ROOT);
    if (NULL == html->root)
    {
        free(html), html = NULL;
        log2_error("Create node failed!");
        return NULL;
    }

    /* 3. Set root name */
    html->root->name = (char *)calloc(1, HTML_ROOT_NAME_SIZE);
    if (NULL == html->root->name)
    {
        html_destroy(html);
        log2_error("Calloc failed!");
        return NULL;
    }
    
    snprintf(html->root->name, HTML_ROOT_NAME_SIZE, "%s", HTML_ROOT_NAME);

    return html;
}

/******************************************************************************
 **函数名称: html_creat
 **功    能: 将HTML文件转化成HTML树
 **输入参数:
 **     log: 日志对象
 **     fcache: 文件缓存
 **输出参数:
 **返    回: HTML树
 **实现描述: 
 **     1. 将HTML文件读入内存
 **     2. 在内存中将HTML文件转为HTML树
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
html_tree_t *html_creat(const char *fname)
{
    char *buff = NULL;
    html_tree_t *html = NULL;

    /* 1. 将HTML文件读入内存 */
    buff = html_fload(fname);
    if (NULL == buff)
    {
        log2_error("Load html file into memory failed![%s]", fname);
        return NULL;
    }

    /* 2. 在内存中将HTML文件转为HTML树 */
    html = html_screat(buff);

    free(buff), buff = NULL;

    return html;
}

/******************************************************************************
 **函数名称: html_screat_ext
 **功    能: 将HTML字串转为HTML树
 **输入参数:
 **     str: HTML字串
 **     length: 字串长度
 **输出参数:
 **返    回: HTML树
 **实现描述: 
 **     1. 分配缓存空间
 **     2. 截取HTML字串
 **     3. 解析为HTML字串
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.09.25 #
 ******************************************************************************/
html_tree_t *html_screat_ext(const char *str, int length)
{
    char *buff = NULL;
    html_tree_t *html = NULL;

    if (0 == length)     /* 创建空树 */
    {
        return html_creat_empty(); 
    }
    else if (length < 0) /* 长度无限制 */
    {
        return html_screat(str);
    }

    /* length > 0 */
    buff = (char *)calloc(1, length + 1);
    if (NULL == buff)
    {
        log2_error("Alloc memory failed!");
        return NULL;
    }

    memcpy(buff, str, length);

    html = html_screat(buff);

    free(buff), buff = NULL;

    return html;
}

/******************************************************************************
 **函数名称: html_screat
 **功    能: 将HTML字串转为HTML树
 **输入参数:
 **     str: HTML字串
 **输出参数:
 **返    回: HTML树
 **实现描述: 
 **     1. 初始化栈
 **     2. 初始化html树
 **     3. 在内存中将文件解析为HTML树
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
 html_tree_t *html_screat(const char *str)
{
    int ret = 0;
    Stack_t stack;
    html_tree_t *html = NULL;

    if ((NULL == str)
        || ('\0' == str[0]))
    {
        return html_creat_empty();
    }
    
    do
    {
        /* 1. 初始化栈 */
        ret = stack_init(&stack, HTML_MAX_DEPTH);
        if (HTML_OK != ret)
        {
            log2_error("Init html stack failed!");
            break;
        }

        /* 2. 初始化HTML树 */
        ret = html_init(&html);
        if (HTML_OK != ret)
        {   
            log2_error("Init html failed!");
            break;
        }

        /* 3. 解析HTML文件缓存 */
        ret = html_parse(html, &stack, str);
        if (HTML_OK != ret)
        {
            log2_error("Parse html failed!");
            html_destroy(html);
            break;
        }
    }while (0);

    /* 4. 释放内存空间 */
    stack_destroy(&stack);
    
    return html;
}

/* 释放属性节点 */
#define html_attr_free(node, child) \
{   \
    if (html_has_attr(node))    \
    {   \
        while (NULL != node->temp)   \
        {   \
            child = node->temp; \
            if (html_is_attr(child))    \
            {   \
                node->temp = child->next;   \
                html_node_sfree(child), child = NULL;    \
                continue;   \
            }   \
            node->firstchild = node->temp; /* 让孩子指针指向真正的孩子节点 */  \
            break;  \
        }   \
    }   \
}

/******************************************************************************
 **函数名称: html_node_free
 **功    能: 释放指定节点，及其所有属性节点、子节点的内存
 **输入参数:
 **     html: 
 **     node: 被释放的节点
 **输出参数:
 **返    回: 0: 成功 !0: 失败
 **实现描述: 
 **     1. 将孩子从链表中剔除
 **     2. 释放孩子节点及其所有子节点
 **注意事项: 
 **     除释放指定节点的内存外，还必须释放该节点所有子孙节点的内存
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_node_free(html_tree_t *html, html_node_t *node)
{
    int ret = 0;
    Stack_t _stack, *stack = &_stack;
    html_node_t *current = node,
               *parent = node->parent, *child = NULL;

    /* 1. 将此节点从孩子链表剔除 */
    if ((NULL != parent) && (NULL != current))
    {
        ret = html_delete_child(html, parent, node);
        if (HTML_OK != ret)
        {
            return ret;
        }
    }

    ret = stack_init(stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Init stack failed!");
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 节点入栈 */
        current->temp = current->firstchild;
        ret = stack_push(stack, current);
        if (HTML_OK != ret)
        {
            stack_destroy(stack);
            log2_error("Push stack failed!");
            return HTML_ERR_STACK;
        }

        /* 2. 释放属性节点: 让孩子指针指向真正的孩子节点 */
        html_attr_free(current, child);


        /* 3. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        current = html_free_next(html, stack, current); 
        
    }while (NULL != current);

    if (!stack_isempty(stack))
    {
        stack_destroy(stack);
        log2_error("Stack is not empty!");
        return HTML_ERR_STACK;
    }

    stack_destroy(stack);
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_fprint
 **功    能: 根据HTML树构建HTML文件(注: HTML有层次格式)
 **输入参数:
 **     html: HTML树
 **     fp: 文件指针
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.03.26 #
 ******************************************************************************/
int html_fprint(html_tree_t *html, FILE *fp)
{
    int ret = 0;
    Stack_t stack;
    html_node_t *child = html->root->firstchild;

    if (NULL == child) 
    {
        log2_error("The tree is empty!");
        return HTML_ERR_EMPTY_TREE;
    }
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Stack init failed!");
        return HTML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = html_fprint_tree(html, child, &stack, fp);
        if (HTML_OK != ret)
        {
            log2_error("fPrint tree failed!");
            stack_destroy(&stack);
            return ret;
        }
        child = child->next;
    }

    stack_destroy(&stack);
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_fwrite
 **功    能: 根据HTML树构建HTML文件(注: HTML有层次格式)
 **输入参数:
 **     html: HTML树
 **     fname: 文件路径
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_fwrite(html_tree_t *html, const char *fname)
{
    int ret = 0;
    Stack_t stack;
    FILE *fp = NULL;
    html_node_t *child = html->root->firstchild;

    if (NULL == child) 
    {
        log2_error("The tree is empty!");
        return HTML_ERR_EMPTY_TREE;
    }

    fp = fopen(fname, "wb");
    if (NULL == fp)
    {
        log2_error("Call fopen() failed![%s]", fname);
        return HTML_ERR_FOPEN;
    }
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        fclose(fp), fp = NULL;
        log2_error("Stack init failed!");
        return HTML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = html_fprint_tree(html, child, &stack, fp);
        if (HTML_OK != ret)
        {
            log2_error("fPrint tree failed!");
            fclose(fp), fp = NULL;
            stack_destroy(&stack);
            return ret;
        }
        child = child->next;
    }

    fclose(fp), fp = NULL;
    stack_destroy(&stack);
    return HTML_OK;
}

/******************************************************************************
 **函数名称: html_sprint
 **功    能: 根据HTML树构建HTML文件缓存(注: HTML有层次格式)
 **输入参数:
 **     html: HTML树
 **     str: 用于存放文件缓存
 **输出参数:
 **返    回: 返回HTML文件缓存的长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_sprint(html_tree_t *html, char *str)
{
    int ret = 0;
    sprint_t sp;
    Stack_t stack;
    html_node_t *child = html->root->firstchild;

    if (NULL == child) 
    {
        return HTML_OK;
    }

    sprint_init(&sp, str);
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Stack init failed!");
        return HTML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = html_sprint_tree(html, child, &stack, &sp);
        if (HTML_OK != ret)
        {
            log2_error("Sprint tree failed!");
            stack_destroy(&stack);
            return ret;
        }
        child = child->next;
    }
    
    stack_destroy(&stack);
    return (sp.ptr - sp.str);
}

/******************************************************************************
 **函数名称: html_spack
 **功    能: 根据HTML树构建HTML报文(注: HTML无层次格式)
 **输入参数:
 **     html: HTML树
 **     str: 用于存放HTML报文
 **输出参数:
 **返    回: 返回HTML文件报文的长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
extern int html_spack(html_tree_t *html, char *str)
{
    int ret = 0;
    sprint_t sp;
    Stack_t stack;
    html_node_t *child = html->root->firstchild;

    if (NULL == child) 
    {
        return HTML_OK;
    }

    sprint_init(&sp, str);
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Stack init failed!");
        return HTML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = html_pack_tree(html, child, &stack, &sp);
        if (HTML_OK != ret)
        {
            log2_error("Sprint tree failed!");
            stack_destroy(&stack);
            return ret;
        }
        child = child->next;
    }
    
    stack_destroy(&stack);
    return (sp.ptr - sp.str);
}

/******************************************************************************
 **函数名称: html_rsearch
 **功    能: 搜索指定节点的信息(相对路径)
 **输入参数:
 **     curr: 参照结点
 **     path: 查找路径
 **输出参数:
 **返    回: 查找到的节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.26 #
 ******************************************************************************/
html_node_t *html_rsearch(html_tree_t *html, html_node_t *curr, const char *path)
{
    size_t len = 0;
    html_node_t *node = curr;
    const char *str = path, *ptr = NULL;

    /* 1. 路径判断 */
    if (HtmlIsRootPath(path))
    {
        return curr;
    }
    else if (HtmlIsAbsPath(path))
    {
        str++;
    }

    node = curr->firstchild;
    if (NULL == node)
    {
        return NULL;
    }

    /* 2. 路径解析处理 */
    do
    {
        /* 2.1 获取节点名长度 */
        ptr = strstr(str, ".");
        if (NULL == ptr)
        {
            len = strlen(str);
        }
        else
        {
            len = ptr - str;
        }
        
        /* 2.2 兄弟节点中查找 */
        while (NULL != node)
        {
            if ((len == strlen(node->name))
                && (0 == strncmp(node->name, str, len)))
            {
                break;
            }
            node = node->next;
        }

        if (NULL == node)
        {
            return NULL;
        }
        else if (NULL == ptr)
        {
            return node;
        }

        str = ptr+1;
        node = node->firstchild;
    }while (NULL != node);
    
    return NULL;
}

/******************************************************************************
 **函数名称: html_add_attr
 **功    能: 往节点中添加属性节点
 **输入参数:
 **     node: 需要添加属性节点的节点
 **     attr: 属性节点(链表或单个节点)
 **输出参数:
 **返    回: 被创建节点的地址
 **实现描述: 
 **     属性节点放在所有属性节点后面
 **注意事项: 
 **     属性节点(attr)可以有兄弟节点
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
html_node_t *html_add_attr(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value)
{
    html_node_t *attr = NULL,
        *parent = node->parent,
        *link = node->firstchild;

    if (NULL == parent)
    {
        log2_error("Please create root node at first!");
        return NULL;
    }

    if (html_is_attr(node))
    {
        log2_error("Can't add attr for attribute node!");
        return NULL;
    }

    /* 1. 创建节点 */
    attr = html_node_creat_ext(HTML_NODE_ATTR, name, value);
    if (NULL == attr)
    {
        log2_error("Create node failed!");
        return NULL;
    }
    
    /* 2. 将节点放入HTML树 */
    if (NULL == link)                    /* 没有孩子节点，也没有属性节点 */
    {
        node->firstchild = attr;
        node->tail = attr;
        attr->parent = node;
        html_set_attr_flag(node);

        return attr;
    }

    if (html_has_attr(node))                  /* 有属性节点 */
    {
        if (html_is_attr(node->tail))         /* 所有子节点也为属性节点时，attr直接链入链表尾 */
        {
            attr->parent = node;
            node->tail->next = attr;
            node->tail = attr;

            html_set_attr_flag(node);
            return attr;
        }
        
        while ((NULL != link->next)              /* 查找最后一个属性节点 */
            &&(html_is_attr(link->next)))
        {
            link = link->next;
        }

        attr->parent = node;
        attr->next = link->next;
        link->next = attr;

        html_set_attr_flag(node);
        return attr;
    }
    else if (html_has_child(node) && !html_has_attr(node)) /* 有孩子但无属性 */
    {    
        attr->parent = node;
        attr->next = node->firstchild;
        node->firstchild = attr;

        html_set_attr_flag(node);
        return attr;
    }

    html_node_sfree(attr);
    
    log2_error("Add attr node failed!");
    return NULL;
}

/******************************************************************************
 **函数名称: html_add_child
 **功    能: 给指定节点添加孩子节点
 **输入参数:
 **     node: 需要添加孩子节点的节点
 **     name: 孩子节点名
 **     value: 孩子节点值
 **输出参数:
 **返    回: 新增节点的地址
 **实现描述: 
 **注意事项: 
 **     1. 新建孩子节点
 **     2. 将孩子加入子节点链表尾
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
html_node_t *html_add_child(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value)
{
    html_node_t *child = NULL;

    if (html_is_attr(node))
    {
        log2_error("Can't add child for attribute node![%s]", node->name);
        return NULL;
    }
#if defined(__HTML_OCOV__)
    else if (html_has_value(node))
    {
        log2_error("Can't add child for the node which has value![%s]", node->name);
        return NULL;
    }
#endif /*__HTML_OCOV__*/

    /* 1. 新建孩子节点 */
    child = html_node_creat_ext(HTML_NODE_CHILD, name, value);
    if (NULL == child)
    {
        log2_error("Create node failed![%s]", name);
        return NULL;
    }

    child->parent = node;

    /* 2. 将孩子加入子节点链表尾 */    
    if (NULL == node->tail)              /* 没有孩子&属性节点 */
    {
        node->firstchild = child;
    }
    else
    {
        node->tail->next = child;
    }

    node->tail = child;

    html_set_child_flag(node);
    
    return child;
}

/******************************************************************************
 **函数名称: html_add_node
 **功    能: 给节点添加属性或孩子节点
 **输入参数:
 **     node: 父节点
 **     name: 新节点名
 **     value: 新节点值
 **     type: 新节点类型. 其取值范围html_node_type_t
 **输出参数:
 **返    回: 新增节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
html_node_t *html_add_node(
        html_tree_t *html, html_node_t *node,
        const char *name, const char *value, int type)
{
    switch(type)
    {
        case HTML_NODE_ATTR:
        {
            return html_add_attr(html, node, name, value);
        }
        case HTML_NODE_ROOT:
        case HTML_NODE_CHILD:
        {
            return html_add_child(html, node, name, value);
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: html_node_length
 **功    能: 计算HTML树打印成HTML格式字串时的长度(注: 有层次结构)
 **输入参数:
 **     html: HTML树
 **     node: HTML节点
 **输出参数:
 **返    回: HTML格式字串长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
int html_node_length(html_tree_t *html, html_node_t *node)
{
    int ret=0, length=0;
    Stack_t stack;
    
    if (NULL == node)
    {
        log2_error("The node is empty!");
        return 0;
    }
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Stack init failed!");
        return -1;
    }

    length = _html_node_length(html, node, &stack);
    if (length < 0)
    {
        log2_error("Get the length of node failed!");
        stack_destroy(&stack);
        return -1;
    }

    stack_destroy(&stack);
    return length;
}

/******************************************************************************
 **函数名称: html_set_value
 **功    能: 设置节点值
 **输入参数:
 **     node: HTML节点
 **     value: 节点值
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
int html_set_value(html_node_t *node, const char *value)
{
    int size = 0;
    
    if (NULL != node->value)
    {
        free(node->value), node->value = NULL;
        html_unset_value_flag(node);
    }

    if ((NULL == value) || ('\0' == value[0]))
    {
        if (html_is_attr(node))
        {
            /* 注意: 属性节点的值不能为NULL，应为“” - 防止计算HTML树长度时，出现计算错误 */
            node->value = (char *)calloc(1, 1);
            if (NULL == node->value)
            {
                return HTML_ERR;
            }
			
            html_set_value_flag(node);
            return HTML_OK;
        }

        html_unset_value_flag(node);
        return HTML_OK;
    }

    size = strlen(value) + 1;
    
    node->value = (char *)calloc(1, size);
    if (NULL == node->value)
    {
        html_unset_value_flag(node);
        return HTML_ERR;
    }

    snprintf(node->value, size, "%s", value);
    html_set_value_flag(node);
    
    return HTML_OK;
}

/******************************************************************************
 **函数名称: _html_pack_length
 **功    能: 计算HTML树打印成HTML报文字串时的长度(注: HTML无层次结构)
 **输入参数:
 **     node: HTML节点
 **输出参数: NONE
 **返    回: 报文长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
int _html_pack_length(html_tree_t *html, html_node_t *node)
{
    int ret = 0, length = 0, length2 = 0;
    Stack_t stack;
    html_node_t *child = NULL;
    
    if (NULL == node)
    {
        log2_error("The node is empty!");
        return 0;
    }
    
    ret = stack_init(&stack, HTML_MAX_DEPTH);
    if (HTML_OK != ret)
    {
        log2_error("Stack init failed!");
        return -1;
    }

    switch(node->type)
    {
        case HTML_NODE_CHILD: /* 处理孩子节点 */
        {
            length = html_pack_node_length(html, node, &stack);
            if (length < 0)
            {
                log2_error("Get length of the node failed!");
                stack_destroy(&stack);
                return -1;
            }
            break;
        }
        case HTML_NODE_ROOT:  /* 处理父亲节点 */
        {
            child = node->firstchild;
            while (NULL != child)
            {
                length2 = html_pack_node_length(html, child, &stack);
                if (length2 < 0)
                {
                    log2_error("Get length of the node failed!");
                    stack_destroy(&stack);
                    return -1;
                }

                length += length2;
                child = child->next;
            }
            break;
        }
        case HTML_NODE_ATTR:
        case HTML_NODE_UNKNOWN:
        {
            /* Do nothing */
            break;
        }
    }

    stack_destroy(&stack);
    return length;
}

/******************************************************************************
 **函数名称: html_delete_empty
 **功    能: 删除无属性节、无孩子、无节点值的节点(注: 不删属性节点)
 **输入参数:
 **     html: HTML树
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **     1. 初始化栈
 **     2. 节点属性判断: 
 **注意事项: 
 **     1. 属性节点不用入栈
 **     2. 用于孩子节点的节点需要入栈
 **     3. 如为无属性节点、无孩子节点、且无节点值的节点，则删除之
 **作    者: # Qifeng.zou # 2013.10.21 #
 ******************************************************************************/
int html_delete_empty(html_tree_t *html)
{
    int ret = -1;
    html_node_t *node = NULL;
    Stack_t _stack, *stack = &_stack;


    ret = stack_init(stack, HTML_MAX_DEPTH);
    if (0 != ret)
    {
        log2_error("Init stack failed!");
        return HTML_ERR_STACK;
    }

    node = html->root->firstchild;
    while (NULL != node)
    {
        /* 1. 此节点为属性节点: 不用入栈, 继续查找其兄弟节点 */
        if (html_is_attr(node))
        {
            if (NULL != node->next)
            {
                node = node->next;
                continue;
            }

            /* 属性节点后续无孩子节点: 说明其父节点无孩子节点, 此类父节点不应该入栈 */
            log2_error("Push is not right!");
            return HTML_ERR_STACK;
        }
        /* 2. 此节点有孩子节点: 入栈, 并处理其孩子节点 */
        else if (html_has_child(node))
        {
            ret = stack_push(stack, node);
            if (0 != ret)
            {
                log2_error("Push failed!");
                return HTML_ERR_STACK;
            }
            
            node = node->firstchild;
            continue;
        }
        /* 3. 此节点为拥有节点值或属性节点, 而无孩子节点: 此节点不入栈, 并继续查找其兄弟节点 */
        else if (html_has_value(node) || html_has_attr(node))
        {
            do
            {
                /* 3.1 查找兄弟节点: 处理自己的兄弟节点 */
                if (NULL != node->next)
                {
                    node = node->next;
                    break;
                }

                /* 3.2 已无兄弟节点: 则处理父节点的兄弟节点 */
                node = stack_gettop(stack);

                stack_pop(stack);
            }while (1);
            continue;
        }
        /* 4. 删除无属性、无孩子、无节点值的节点 */
        else /* if (!html_has_attr(node) && !html_has_child(node) && !html_has_value(node) && !html_is_attr(node)) */
        {
            node = _html_delete_empty(html, stack, node);
        }
    }

    stack_destroy(stack);
    
    return HTML_OK;
}

/******************************************************************************
 **函数名称: _html_delete_empty
 **功    能: 删除无属性节、无孩子、无节点值的节点，同时返回下一个需要处理的节点(注: 不删属性节点)
 **输入参数:
 **     html: HTML树
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     注意: node节点必须为子节点，否则处理过程的判断条件会有错误!!!
 **作    者: # Qifeng.zou # 2013.10.21 #
 ******************************************************************************/
static html_node_t *_html_delete_empty(html_tree_t *html, Stack_t *stack, html_node_t *node)
{
    html_node_t *parent = NULL, *prev = NULL;

    
    do
    {
        parent = node->parent;
        prev = parent->firstchild;

        if (prev == node)
        {
            parent->firstchild = node->next;
            
            html_node_sfree(node);           /* 释放空节点 */
            
            if (NULL != parent->firstchild)
            {
                return parent->firstchild;  /* 处理子节点的兄弟节点 */
            }
            
            /* 已无兄弟: 则处理父节点 */
            html_unset_child_flag(parent);
            /* 继续后续处理 */
        }
        else
        {
            while (prev->next != node)
            {
                prev = prev->next;
            }
            prev->next = node->next;
            
            html_node_sfree(node);   /* 释放空节点 */
            
            if (NULL != prev->next)
            {
                return prev->next;  /* 还有兄弟: 则处理后续节点 */
            }
            else
            {
                /* 已无兄弟: 则处理父节点 */
                if (html_is_attr(prev))
                {
                    html_unset_child_flag(parent);
                }
                /* 继续后续处理 */
            }
        }

        /* 开始处理父节点 */
        node = parent;
        
        stack_pop(stack);

        /* 删除无属性、无孩子、无节点值的节点 */
        if (!html_has_attr(node) && !html_has_value(node) && !html_has_child(node))
        {
            continue;
        }

        if (NULL != node->next)
        {
            return node->next; /* 处理父节点的兄弟节点 */
        }

        node = stack_gettop(stack);
        
        stack_pop(stack);
    }while (NULL != node);

    return NULL;
}
