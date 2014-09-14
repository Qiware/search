/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: xml_tree.c
 ** 版本号: 1.0
 ** 描  述: XML的处理
 **         这此文件中主要包含了XML处理的对外接口
 ** 作  者: # Qifeng.zou # 2013.02.18 #
 ******************************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "xml_tree.h"
#include "xml_comm.h"
#include "common.h"
#include "log.h"


/* 是否为根路径 */
#define XmlIsRootPath(path) (0 == strcmp(path, "."))

/* 是否为绝对路径 */
#define XmlIsAbsPath(path) ('.' == path[0])

static xml_node_t *_xml_delete_empty(xml_tree_t *xml, Stack_t *stack, xml_node_t *node);

/******************************************************************************
 **函数名称: xml_creat_empty
 **功    能: 创建空XML树
 **输入参数: NONE
 **输出参数: NONE
 **返    回: XML树
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
xml_tree_t *xml_creat_empty(void)
{
    xml_tree_t *xml = NULL;
#if defined(__XML_MEM_POOL__)
    mem_pool_t *pool;

    /* 1. 创建内存池 */
    pool = mem_pool_creat(4096);
    if (NULL == pool)
    {
        log2_error("Create memory pool failed!");
        return NULL;
    }
#endif /*__XML_MEM_POOL__*/

    /* 1. Create tree */
#if defined(__XML_MEM_POOL__)
    xml = (xml_tree_t*)mem_pool_calloc(pool, sizeof(xml_tree_t));
#else /*!__XML_MEM_POOL__*/
    xml = (xml_tree_t*)calloc(1, sizeof(xml_tree_t));
#endif /*!__XML_MEM_POOL__*/
    if (NULL == xml)
    {
    #if defined(__XML_MEM_POOL__)
        mem_pool_destroy(pool);
    #endif /*__XML_MEM_POOL__*/
        log2_error("Calloc failed!");
        return NULL;
    }

#if defined(__XML_MEM_POOL__)
    xml->pool = pool;
#endif /*__XML_MEM_POOL__*/

    /* 2. Add root node */
    xml->root = xml_node_creat(xml, XML_NODE_ROOT);
    if (NULL == xml->root)
    {
        xml_destroy(xml);
        log2_error("Create node failed!");
        return NULL;
    }

    /* 3. Set root name */
#if defined(__XML_MEM_POOL__)
    xml->root->name = (char *)mem_pool_calloc(pool, XML_ROOT_NAME_SIZE);
#else /*!__XML_MEM_POOL__*/
    xml->root->name = (char *)calloc(1, XML_ROOT_NAME_SIZE);
#endif /*!__XML_MEM_POOL__*/
    if (NULL == xml->root->name)
    {
        xml_destroy(xml);
        log2_error("Calloc failed!");
        return NULL;
    }
    
    snprintf(xml->root->name, XML_ROOT_NAME_SIZE, "%s", XML_ROOT_NAME);

    return xml;
}

/******************************************************************************
 **函数名称: xml_creat
 **功    能: 将XML文件转化成XML树
 **输入参数:
 **     fcache: 文件缓存
 **输出参数:
 **返    回: XML树
 **实现描述: 
 **     1. 将XML文件读入内存
 **     2. 在内存中将XML文件转为XML树
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
xml_tree_t *xml_creat(const char *fname)
{
    char *buff = NULL;
    xml_tree_t *xml = NULL;

    /* 1. 将XML文件读入内存 */
    buff = xml_fload(fname);
    if (NULL == buff)
    {
        log2_error("Load xml file into memory failed![%s]", fname);
        return NULL;
    }

    /* 2. 在内存中将XML文件转为XML树 */
    xml = xml_screat(buff);

    free(buff), buff = NULL;

    return xml;
}

/******************************************************************************
 **函数名称: xml_screat_ext
 **功    能: 将XML字串转为XML树
 **输入参数:
 **     str: XML字串
 **     length: 字串长度
 **输出参数:
 **返    回: XML树
 **实现描述: 
 **     1. 分配缓存空间
 **     2. 截取XML字串
 **     3. 解析为XML字串
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.09.25 #
 ******************************************************************************/
xml_tree_t *xml_screat_ext(const char *str, int length)
{
    char *buff = NULL;
    xml_tree_t *xml = NULL;

    if (0 == length)     /* 创建空树 */
    {
        return xml_creat_empty(); 
    }
    else if (length < 0) /* 长度无限制 */
    {
        return xml_screat(str);
    }

    /* length > 0 */
    buff = (char *)calloc(1, length + 1);
    if (NULL == buff)
    {
        log2_error("Alloc memory failed!");
        return NULL;
    }

    memcpy(buff, str, length);

    xml = xml_screat(buff);

    free(buff), buff = NULL;

    return xml;
}

/******************************************************************************
 **函数名称: xml_screat
 **功    能: 将XML字串转为XML树
 **输入参数:
 **     str: XML字串
 **输出参数:
 **返    回: XML树
 **实现描述: 
 **     1. 初始化栈
 **     2. 初始化xml树
 **     3. 在内存中将文件解析为XML树
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
xml_tree_t *xml_screat(const char *str)
{
    int ret = 0;
    Stack_t stack;
    xml_tree_t *xml = NULL;

    if ((NULL == str)
        || ('\0' == str[0]))
    {
        return xml_creat_empty();
    }
    
    do
    {
        /* 1. 初始化栈 */
        ret = stack_init(&stack, XML_MAX_DEPTH);
        if (XML_OK != ret)
        {
            log2_error("Init xml stack failed!");
            break;
        }

        /* 2. 初始化XML树 */
        ret = xml_init(&xml);
        if (XML_OK != ret)
        {   
            log2_error("Init xml tree failed!");
            break;
        }

        /* 3. 解析XML文件缓存 */
        ret = xml_parse(xml, &stack, str);
        if (XML_OK != ret)
        {
            log2_error("Parse xml failed!");
            xml_destroy(xml);
            break;
        }
    }while (0);

    /* 4. 释放内存空间 */
    stack_destroy(&stack);
    
    return xml;
}

#if !defined(__XML_MEM_POOL__)
/* 释放属性节点 */
#define xml_attr_free(node, child) \
{   \
    if (xml_has_attr(node))    \
    {   \
        while (NULL != node->temp)   \
        {   \
            child = node->temp; \
            if (xml_is_attr(child))    \
            {   \
                node->temp = child->next;   \
                xml_node_free_one(child), child = NULL;    \
                continue;   \
            }   \
            node->firstchild = node->temp; /* 让孩子指针指向真正的孩子节点 */  \
            break;  \
        }   \
    }   \
}

/******************************************************************************
 **函数名称: xml_node_free
 **功    能: 释放指定节点，及其所有属性节点、子节点的内存
 **输入参数:
 **     xml: 
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
int xml_node_free(xml_tree_t *xml, xml_node_t *node)
{
    int ret = 0;
    Stack_t _stack, *stack = &_stack;
    xml_node_t *current = node,
               *parent = node->parent, *child = NULL;

    /* 1. 将此节点从孩子链表剔除 */
    if ((NULL != parent) && (NULL != current))
    {
        ret = xml_delete_child(xml, parent, node);
        if (XML_OK != ret)
        {
            return ret;
        }
    }

    ret = stack_init(stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Init stack failed!");
        return XML_ERR_STACK;
    }

    do
    {
        /* 1. 节点入栈 */
        current->temp = current->firstchild;
        ret = stack_push(stack, current);
        if (XML_OK != ret)
        {
            stack_destroy(stack);
            log2_error("Push stack failed!");
            return XML_ERR_STACK;
        }

        /* 2. 释放属性节点: 让孩子指针指向真正的孩子节点 */
        xml_attr_free(current, child);


        /* 3. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        current = xml_free_next(xml, stack, current); 
        
    }while (NULL != current);

    if (!stack_isempty(stack))
    {
        stack_destroy(stack);
        log2_error("Stack is not empty!");
        return XML_ERR_STACK;
    }

    stack_destroy(stack);
    return XML_OK;
}
#endif /*!__XML_MEM_POOL__*/

/******************************************************************************
 **函数名称: xml_fprint
 **功    能: 根据XML树构建XML文件(注: XML有层次格式)
 **输入参数:
 **     xml: XML树
 **     fp: 文件指针
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.03.26 #
 ******************************************************************************/
int xml_fprint(xml_tree_t *xml, FILE *fp)
{
    int ret = 0;
    Stack_t stack;
    xml_node_t *child = xml->root->firstchild;

    if (NULL == child) 
    {
        log2_error("The tree is empty!");
        return XML_ERR_EMPTY_TREE;
    }
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Stack init failed!");
        return XML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = xml_fprint_tree(xml, child, &stack, fp);
        if (XML_OK != ret)
        {
            log2_error("fPrint tree failed!");
            stack_destroy(&stack);
            return ret;
        }
        child = child->next;
    }

    stack_destroy(&stack);
    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_fwrite
 **功    能: 根据XML树构建XML文件(注: XML有层次格式)
 **输入参数:
 **     xml: XML树
 **     fname: 文件路径
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int xml_fwrite(xml_tree_t *xml, const char *fname)
{
    int ret = 0;
    Stack_t stack;
    FILE *fp = NULL;
    xml_node_t *child = xml->root->firstchild;

    if (NULL == child) 
    {
        log2_error("The tree is empty!");
        return XML_ERR_EMPTY_TREE;
    }

    fp = fopen(fname, "wb");
    if (NULL == fp)
    {
        log2_error("Call fopen() failed![%s]", fname);
        return XML_ERR_FOPEN;
    }
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        fclose(fp), fp = NULL;
        log2_error("Stack init failed!");
        return XML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = xml_fprint_tree(xml, child, &stack, fp);
        if (XML_OK != ret)
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
    return XML_OK;
}

/******************************************************************************
 **函数名称: xml_sprint
 **功    能: 根据XML树构建XML文件缓存(注: XML有层次格式)
 **输入参数:
 **     xml: XML树
 **     str: 用于存放文件缓存
 **输出参数:
 **返    回: 返回XML文件缓存的长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int xml_sprint(xml_tree_t *xml, char *str)
{
    int ret = 0;
    sprint_t sp;
    Stack_t stack;
    xml_node_t *child = xml->root->firstchild;

    if (NULL == child) 
    {
        return XML_OK;
    }

    sprint_init(&sp, str);
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Stack init failed!");
        return XML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = xml_sprint_tree(xml, child, &stack, &sp);
        if (XML_OK != ret)
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
 **函数名称: xml_spack
 **功    能: 根据XML树构建XML报文(注: XML无层次格式)
 **输入参数:
 **     xml: XML树
 **     str: 用于存放XML报文
 **输出参数:
 **返    回: 返回XML文件报文的长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
extern int xml_spack(xml_tree_t *xml, char *str)
{
    int ret = 0;
    sprint_t sp;
    Stack_t stack;
    xml_node_t *child = xml->root->firstchild;

    if (NULL == child) 
    {
        return XML_OK;
    }

    sprint_init(&sp, str);
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Stack init failed!");
        return XML_ERR_STACK;
    }

    while (NULL != child)
    {
        ret = xml_pack_tree(xml, child, &stack, &sp);
        if (XML_OK != ret)
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
 **函数名称: xml_rsearch
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
xml_node_t *xml_rsearch(xml_tree_t *xml, xml_node_t *curr, const char *path)
{
    size_t len = 0;
    xml_node_t *node = curr;
    const char *str = path, *ptr = NULL;

    /* 1. 路径判断 */
    if (XmlIsRootPath(path))
    {
        return curr;
    }
    else if (XmlIsAbsPath(path))
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
 **函数名称: xml_add_attr
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
xml_node_t *xml_add_attr(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value)
{
    xml_node_t *attr = NULL,
        *parent = node->parent,
        *link = node->firstchild;

    if (NULL == parent)
    {
        log2_error("Please create root node at first!");
        return NULL;
    }

    if (xml_is_attr(node))
    {
        log2_error("Can't add attr for attribute node!");
        return NULL;
    }

    /* 1. 创建节点 */
    attr = xml_node_creat_ext(xml, XML_NODE_ATTR, name, value);
    if (NULL == attr)
    {
        log2_error("Create node failed!");
        return NULL;
    }
    
    /* 2. 将节点放入XML树 */
    if (NULL == link)                    /* 没有孩子节点，也没有属性节点 */
    {
        node->firstchild = attr;
        node->tail = attr;
        attr->parent = node;
        xml_set_attr_flag(node);

        return attr;
    }

    if (xml_has_attr(node))                  /* 有属性节点 */
    {
        if (xml_is_attr(node->tail))         /* 所有子节点也为属性节点时，attr直接链入链表尾 */
        {
            attr->parent = node;
            node->tail->next = attr;
            node->tail = attr;

            xml_set_attr_flag(node);
            return attr;
        }
        
        while ((NULL != link->next)              /* 查找最后一个属性节点 */
            &&(xml_is_attr(link->next)))
        {
            link = link->next;
        }

        attr->parent = node;
        attr->next = link->next;
        link->next = attr;

        xml_set_attr_flag(node);
        return attr;
    }
    else if (xml_has_child(node) && !xml_has_attr(node)) /* 有孩子但无属性 */
    {    
        attr->parent = node;
        attr->next = node->firstchild;
        node->firstchild = attr;

        xml_set_attr_flag(node);
        return attr;
    }

#if !defined(__XML_MEM_POOL__)
    xml_node_free_one(attr);
#endif /*__XML_MEM_POOL__*/
    
    log2_error("Add attr node failed!");
    return NULL;
}

/******************************************************************************
 **函数名称: xml_add_child
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
xml_node_t *xml_add_child(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value)
{
    xml_node_t *child = NULL;

    if (xml_is_attr(node))
    {
        log2_error("Can't add child for attribute node![%s]", node->name);
        return NULL;
    }
#if defined(__XML_EITHER_CHILD_OR_VALUE__)
    else if (xml_has_value(node))
    {
        log2_error("Can't add child for the node which has value![%s]", node->name);
        return NULL;
    }
#endif /*__XML_EITHER_CHILD_OR_VALUE__*/

    /* 1. 新建孩子节点 */
    child = xml_node_creat_ext(xml, XML_NODE_CHILD, name, value);
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

    xml_set_child_flag(node);
    
    return child;
}

/******************************************************************************
 **函数名称: xml_add_node
 **功    能: 给节点添加属性或孩子节点
 **输入参数:
 **     node: 父节点
 **     name: 新节点名
 **     value: 新节点值
 **     type: 新节点类型. 其取值范围xml_node_type_t
 **输出参数:
 **返    回: 新增节点地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
xml_node_t *xml_add_node(
        xml_tree_t *xml, xml_node_t *node,
        const char *name, const char *value, int type)
{
    switch(type)
    {
        case XML_NODE_ATTR:
        {
            return xml_add_attr(xml, node, name, value);
        }
        case XML_NODE_ROOT:
        case XML_NODE_CHILD:
        {
            return xml_add_child(xml, node, name, value);
        }
    }

    return NULL;
}

/******************************************************************************
 **函数名称: xml_node_length
 **功    能: 计算XML树打印成XML格式字串时的长度(注: 有层次结构)
 **输入参数:
 **     xml: XML树
 **     node: XML节点
 **输出参数:
 **返    回: XML格式字串长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.10 #
 ******************************************************************************/
int xml_node_length(xml_tree_t *xml, xml_node_t *node)
{
    int ret=0, length=0;
    Stack_t stack;
    
    if (NULL == node)
    {
        log2_error("The node is empty!");
        return 0;
    }
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Stack init failed!");
        return -1;
    }

    length = _xml_node_length(xml, node, &stack);
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
 **函数名称: xml_set_value
 **功    能: 设置节点值
 **输入参数:
 **     xml: XML树
 **     node: XML节点
 **     value: 节点值
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.12 #
 ******************************************************************************/
int xml_set_value(xml_tree_t *xml, xml_node_t *node, const char *value)
{
    int size = 0;
    
    if (NULL != node->value)
    {
    #if !defined(__XML_MEM_POOL__)
        free(node->value), node->value = NULL;
    #endif /*__XML_MEM_POOL__*/
        xml_unset_value_flag(node);
    }

    if ((NULL == value) || ('\0' == value[0]))
    {
        if (xml_is_attr(node))
        {
            /* 注意: 属性节点的值不能为NULL，应为“” - 防止计算XML树长度时，出现计算错误 */
        #if defined(__XML_MEM_POOL__)
            node->value = (char *)mem_pool_calloc(xml->pool, sizeof(char));
        #else /*!__XML_MEM_POOL__*/
            node->value = (char *)calloc(1, sizeof(char));
        #endif /*!__XML_MEM_POOL__*/
            if (NULL == node->value)
            {
                return XML_ERR;
            }
			
            xml_set_value_flag(node);
            return XML_OK;
        }

        xml_unset_value_flag(node);
        return XML_OK;
    }

    size = strlen(value) + 1;
    
#if defined(__XML_MEM_POOL__)
    node->value = (char *)mem_pool_calloc(xml->pool, size);
#else /*!__XML_MEM_POOL__*/
    node->value = (char *)calloc(1, size);
#endif /*!__XML_MEM_POOL__*/
    if (NULL == node->value)
    {
        xml_unset_value_flag(node);
        return XML_ERR;
    }

    snprintf(node->value, size, "%s", value);
    xml_set_value_flag(node);
    
    return XML_OK;
}

/******************************************************************************
 **函数名称: _xml_pack_length
 **功    能: 计算XML树打印成XML报文字串时的长度(注: XML无层次结构)
 **输入参数:
 **     node: XML节点
 **输出参数: NONE
 **返    回: 报文长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
int _xml_pack_length(xml_tree_t *xml, xml_node_t *node)
{
    int ret = 0, length = 0, length2 = 0;
    Stack_t stack;
    xml_node_t *child = NULL;
    
    if (NULL == node)
    {
        log2_error("The node is empty!");
        return 0;
    }
    
    ret = stack_init(&stack, XML_MAX_DEPTH);
    if (XML_OK != ret)
    {
        log2_error("Stack init failed!");
        return -1;
    }

    switch(node->type)
    {
        case XML_NODE_CHILD: /* 处理孩子节点 */
        {
            length = xml_pack_node_length(xml, node, &stack);
            if (length < 0)
            {
                log2_error("Get length of the node failed!");
                stack_destroy(&stack);
                return -1;
            }
            break;
        }
        case XML_NODE_ROOT:  /* 处理父亲节点 */
        {
            child = node->firstchild;
            while (NULL != child)
            {
                length2 = xml_pack_node_length(xml, child, &stack);
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
        case XML_NODE_ATTR:
        case XML_NODE_UNKNOWN:
        {
            /* Do nothing */
            break;
        }
    }

    stack_destroy(&stack);
    return length;
}

/******************************************************************************
 **函数名称: xml_delete_empty
 **功    能: 删除无属性节、无孩子、无节点值的节点(注: 不删属性节点)
 **输入参数:
 **     xml: XML树
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
int xml_delete_empty(xml_tree_t *xml)
{
    int ret = -1;
    xml_node_t *node = NULL;
    Stack_t _stack, *stack = &_stack;


    ret = stack_init(stack, XML_MAX_DEPTH);
    if (0 != ret)
    {
        log2_error("Init stack failed!");
        return XML_ERR_STACK;
    }

    node = xml->root->firstchild;
    while (NULL != node)
    {
        /* 1. 此节点为属性节点: 不用入栈, 继续查找其兄弟节点 */
        if (xml_is_attr(node))
        {
            if (NULL != node->next)
            {
                node = node->next;
                continue;
            }

            /* 属性节点后续无孩子节点: 说明其父节点无孩子节点, 此类父节点不应该入栈 */
            log2_error("Push is not right!");
            return XML_ERR_STACK;
        }
        /* 2. 此节点有孩子节点: 入栈, 并处理其孩子节点 */
        else if (xml_has_child(node))
        {
            ret = stack_push(stack, node);
            if (0 != ret)
            {
                log2_error("Push failed!");
                return XML_ERR_STACK;
            }
            
            node = node->firstchild;
            continue;
        }
        /* 3. 此节点为拥有节点值或属性节点, 而无孩子节点: 此节点不入栈, 并继续查找其兄弟节点 */
        else if (xml_has_value(node) || xml_has_attr(node))
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
        else /* if (!xml_has_attr(node) && !xml_has_child(node) && !xml_has_value(node) && !xml_is_attr(node)) */
        {
            node = _xml_delete_empty(xml, stack, node);
        }
    }

    stack_destroy(stack);
    
    return XML_OK;
}

/******************************************************************************
 **函数名称: _xml_delete_empty
 **功    能: 删除无属性节、无孩子、无节点值的节点，同时返回下一个需要处理的节点(注: 不删属性节点)
 **输入参数:
 **     xml: XML树
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **     注意: node节点必须为子节点，否则处理过程的判断条件会有错误!!!
 **作    者: # Qifeng.zou # 2013.10.21 #
 ******************************************************************************/
static xml_node_t *_xml_delete_empty(xml_tree_t *xml, Stack_t *stack, xml_node_t *node)
{
    xml_node_t *parent = NULL, *prev = NULL;

    
    do
    {
        parent = node->parent;
        prev = parent->firstchild;

        if (prev == node)
        {
            parent->firstchild = node->next;
            
        #if !defined(__XML_MEM_POOL__)
            xml_node_free_one(node);           /* 释放空节点 */
        #endif /*!__XML_MEM_POOL__*/            
            
            if (NULL != parent->firstchild)
            {
                return parent->firstchild;  /* 处理子节点的兄弟节点 */
            }
            
            /* 已无兄弟: 则处理父节点 */
            xml_unset_child_flag(parent);
            /* 继续后续处理 */
        }
        else
        {
            while (prev->next != node)
            {
                prev = prev->next;
            }
            prev->next = node->next;
            
        #if !defined(__XML_MEM_POOL__)
            xml_node_free_one(node);   /* 释放空节点 */
        #endif /*!__XML_MEM_POOL__*/
            
            if (NULL != prev->next)
            {
                return prev->next;  /* 还有兄弟: 则处理后续节点 */
            }
            else
            {
                /* 已无兄弟: 则处理父节点 */
                if (xml_is_attr(prev))
                {
                    xml_unset_child_flag(parent);
                }
                /* 继续后续处理 */
            }
        }

        /* 开始处理父节点 */
        node = parent;
        
        stack_pop(stack);

        /* 删除无属性、无孩子、无节点值的节点 */
        if (!xml_has_attr(node) && !xml_has_value(node) && !xml_has_child(node))
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
