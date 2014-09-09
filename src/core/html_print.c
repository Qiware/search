/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: html_print.c
 ** 版本号: 1.0
 ** 描  述: HTML格式的打印
 **        将HTML树打印成HTML格式的文件、字符串等.
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

/******************************************************************************
 **                                                                         **
 **                             打印成有层次结构                            **
 **                                (文件中)                                 **
 ******************************************************************************/
/* 打印节点名(注: HTML有层次格式) */
#define html_fprint_name(fp, node, depth) \
{ \
    while (depth > 1) \
    {   \
        fprintf(fp, "\t"); \
        depth--; \
    } \
    fprintf(fp, "<%s", node->name); \
}

/* 打印属性节点(注: HTML有层次格式) */
#define html_fprint_attr(fp, node) \
{ \
    while (NULL != node->temp) \
    { \
        if (html_is_attr(node->temp)) \
        { \
            fprintf(fp, " %s=\"%s\"", node->temp->name, node->temp->value); \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

/* 打印节点值(注: HTML有层次格式) */
#define html_fprint_value(fp, node) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            fprintf(fp, ">%s\n", node->value); \
        } \
        else \
        { \
            fprintf(fp, ">%s</%s>\n", node->value, node->name); \
        } \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            fprintf(fp, ">\n"); \
        } \
        else \
        { \
            fprintf(fp, "/>\n"); \
        } \
    } \
}

/******************************************************************************
 **函数名称: html_fprint_next
 **功    能: 选择下一个处理的节点(注: HTML有层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     fp: 文件指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
static html_node_t *html_fprint_next(
        html_tree_t *html, Stack_t *stack, html_node_t *node, FILE *fp)
{
    int ret = 0, depth = 0, level = 0;
    html_node_t *top = NULL, *child = NULL;

    if (NULL != node->temp)      /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }
    else                        /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点 */
        top = stack_gettop(stack);
        if (html_has_child(top))
        {
            depth = stack_depth(stack);
            level = depth - 1;
            while (level > 1)
            {
                fprintf(fp, "\t");
                level--;
            }
            fprintf(fp, "</%s>\n", top->name);
        }
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            log_error(html->log, "Compelte fprint!");
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        node = top->next;
        while (NULL == node)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                log_error(html->log, "Stack pop failed!");
                return NULL;
            }
        
            /* 4. 打印父亲节点结束标志 */
            if (html_has_child(top))
            {
                depth = stack_depth(stack);
                level = depth + 1;
                while (level > 1)
                {
                    fprintf(fp, "\t");
                    level--;
                }
                fprintf(fp, "</%s>\n", top->name);
            }
            
            if (stack_isempty(stack))
            {
                return NULL;    /* 处理完成 */
            }

            /* 5. 选择父亲的兄弟节点 */
            node = top->next;
        }
    }    
    
    return node;
}

/******************************************************************************
 **函数名称: html_fprint_tree
 **功    能: 将栈中HTML节点的相关信息打印至文件(注: HTML有层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     fp: 文件指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_fprint_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, FILE *fp)
{
    int ret = 0, depth = 0;
    html_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth)
    {
        log_error(html->log, "Stack depth must empty. depth:[%d]", depth);
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 将要处理的节点压栈 */
        node->temp = node->firstchild;
        ret = stack_push(stack, node);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack push failed!");
            return HTML_ERR_STACK;
        }
        
        /* 2. 打印节点名 */
        depth = stack_depth(stack);
        
        html_fprint_name(fp, node, depth);
        
        /* 3. 打印属性节点 */
        if (html_has_attr(node))
        {
            html_fprint_attr(fp, node);
        }
        
        /* 4. 打印节点值 */
        html_fprint_value(fp, node);
        
        /* 5. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        node = html_fprint_next(html, stack, node, fp);
    }while (NULL != node);

    if (!stack_isempty(stack))
    {
        return HTML_ERR_STACK;
    }
    return HTML_OK;
}

/******************************************************************************
 **                                                                          **
 **                              打印成无层次结构                            **
 **                                 (内存中)                                 **
 ******************************************************************************/
/* 组包节点名的长度(注: HTML无层次格式) */
#define html_pack_name_length(node, length) \
{ \
    /*fprintf(fp, "<%s", node->name);*/ \
    length += (1 + strlen(node->name)); \
}

/* 组包属性节点的长度(注: HTML无层次格式) */
#define html_pack_attr_length(node, length) \
{ \
    while (NULL != node->temp) \
    { \
        if (html_is_attr(node->temp)) \
        { \
            /* sprintf(sp->ptr, " %s=\"%s\"", node->temp->name, node->temp->value); */ \
            /* sp->ptr += strlen(sp->ptr); */ \
            length += strlen(node->temp->name) + strlen(node->temp->value) + 4; \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

#if defined(__HTML_PACK_CMARK__)
/* 组包节点值的长度(注: HTML无层次格式) */
#define html_pack_value_length(node, length) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* sprintf(sp->ptr, ">%s", node->value); */ \
            length += strlen(node->value)+1; \
        } \
        else \
        { \
            /* sprintf(sp->ptr, ">%s</%s>", node->value, node->name); */ \
            length += strlen(node->value) + strlen(node->name) + 4; \
        } \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* sprintf(sp->ptr, ">"); */ \
            length++; \
        } \
        else \
        { \
            /* sprintf(sp->ptr, "></%s>", node->name); */ \
            length += strlen(node->name) + 4; \
        } \
    } \
}
#else /*__HTML_PACK_CMARK__*/
/* 组包节点值的长度(注: HTML无层次格式) */
#define html_pack_value_length(node, length) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* sprintf(sp->ptr, ">%s", node->value); */ \
            length += strlen(node->value)+1; \
        } \
        else \
        { \
            /* sprintf(sp->ptr, ">%s</%s>", node->value, node->name); */ \
            length += strlen(node->value) + strlen(node->name) + 4; \
        } \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            /* sprintf(sp->ptr, ">"); */ \
            length++; \
        } \
        else \
        { \
            /* sprintf(sp->ptr, "/>"); */ \
            length += 2; \
        } \
    } \
}
#endif /*__HTML_PACK_CMARK__*/

/******************************************************************************
 **函数名称: html_pack_next_length
 **功    能: 选择下一个处理的节点, 并计算组包结束节点报文的长度(注: HTML无层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     length: 长度统计
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
static html_node_t *html_pack_next_length(
        html_tree_t *html, Stack_t *stack, html_node_t *node, int *length)
{
    int ret = 0, length2 = 0;
    html_node_t *top = NULL, *child = NULL;

    if (NULL != node->temp)      /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }
    else                        /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点 */
        top = stack_gettop(stack);
        if (html_has_child(top))
        {
            /* sprintf(sp->ptr, "</%s>", top->name); */
            length2 += strlen(top->name) + 3;
        }
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            *length += length2;
            log_error(html->log, "Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            *length += length2;
            log_error(html->log, "Compelte fprint!");
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        node = top->next;
        while (NULL == node)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                *length += length2;
                log_error(html->log, "Stack pop failed!");
                return NULL;
            }
        
            /* 4. 打印父亲节点结束标志 */
            if (html_has_child(top))
            {
                /* sprintf(sp->ptr, "</%s>", top->name); */
                length2 += strlen(top->name)+3;
            }
            
            if (stack_isempty(stack))
            {
                *length += length2;
                return NULL;    /* 处理完成 */
            }

            /* 5. 选择父亲的兄弟节点 */
            node = top->next;
        }
    }    

    *length += length2;
    return node;
}

/******************************************************************************
 **函数名称: html_pack_node_length
 **功    能: 计算节点打印成HTML报文字串时的长度(注: HTML无层次结构)
 **输入参数:
 **     root: HTML树根节点
 **     stack: 栈
 **输出参数:
 **返    回: 节点及其属性、孩子节点的总长度
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.06.11 #
 ******************************************************************************/
int html_pack_node_length(html_tree_t *html, html_node_t *root, Stack_t *stack)
{
    int ret = 0, depth = 0, length=0;
    html_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth)
    {
        log_error(html->log, "Stack depth must empty. depth:[%d]", depth);
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 将要处理的节点压栈 */
        node->temp = node->firstchild;
        
        ret = stack_push(stack, node);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack push failed!");
            return HTML_ERR_STACK;
        }
        
        /* 2. 打印节点名 */
        depth = stack_depth(stack);
        
        html_pack_name_length(node, length);
        
        /* 3. 打印属性节点 */
        if (html_has_attr(node))
        {
            html_pack_attr_length(node, length);
        }
        
        /* 4. 打印节点值 */
        html_pack_value_length(node, length);
        
        /* 5. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        node = html_pack_next_length(html, stack, node, &length);
        
    }while (NULL != node);

    if (!stack_isempty(stack))
    {
        return HTML_ERR_STACK;
    }
    return length;
}

/* 打印节点名(注: HTML无层次格式) */
#define html_pack_name(sp, node) \
{ \
    sprintf(sp->ptr, "<%s", node->name);\
    sp->ptr += strlen(sp->ptr);\
}

/* 打印属性节点(注: HTML无层次格式) */
#define html_pack_attr(sp, node) \
{ \
    while (NULL != node->temp) \
    { \
        if (html_is_attr(node->temp)) \
        { \
            sprintf(sp->ptr, " %s=\"%s\"", node->temp->name, node->temp->value); \
            sp->ptr += strlen(sp->ptr); \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

#if defined(__HTML_PACK_CMARK__)
/* 打印节点值(注: HTML无层次格式) */
#define html_pack_value(sp, node) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">%s", node->value); \
        } \
        else \
        { \
            sprintf(sp->ptr, ">%s</%s>", node->value, node->name); \
        } \
        sp->ptr += strlen(sp->ptr); \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">"); \
            sp->ptr += 1; \
        } \
        else \
        { \
            sprintf(sp->ptr, "></%s>", node->name); \
            sp->ptr += strlen(sp->ptr); \
        } \
    } \
}
#else /*__HTML_PACK_CMARK__*/
/* 打印节点值(注: HTML无层次格式) */
#define html_pack_value(sp, node) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">%s", node->value); \
        } \
        else \
        { \
            sprintf(sp->ptr, ">%s</%s>", node->value, node->name); \
        } \
        sp->ptr += strlen(sp->ptr); \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">"); \
            sp->ptr += 1; \
        } \
        else \
        { \
            sprintf(sp->ptr, "/>"); \
            sp->ptr += 2; \
        } \
    } \
}
#endif /*__HTML_PACK_CMARK__*/

/******************************************************************************
 **函数名称: html_pack_next
 **功    能: 选择下一个处理的节点(注: HTML无层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     sp: 缓存指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
static html_node_t *html_pack_next(
        html_tree_t *html, Stack_t *stack, html_node_t *node, sprint_t *sp)
{
    int ret = 0;
    html_node_t *top = NULL, *child = NULL;

    if (NULL != node->temp)      /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }
    else                        /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点 */
        top = stack_gettop(stack);
        if (html_has_child(top))
        {
            sprintf(sp->ptr, "</%s>", top->name);
            sp->ptr += strlen(sp->ptr);
        }
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            log_error(html->log, "Compelte fprint!");
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        node = top->next;
        while (NULL == node)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                log_error(html->log, "Stack pop failed!");
                return NULL;
            }
        
            /* 4. 打印父亲节点结束标志 */
            if (html_has_child(top))
            {
                sprintf(sp->ptr, "</%s>", top->name);
                sp->ptr += strlen(sp->ptr);
            }
            
            if (stack_isempty(stack))
            {
                return NULL;    /* 处理完成 */
            }

            /* 5. 选择父亲的兄弟节点 */
            node = top->next;
        }
    }    
    
    return node;
}

/******************************************************************************
 **函数名称: html_pack_tree
 **功    能: 将栈中HTML节点的相关信息打印至缓存(注: HTML无层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     sp: 缓存指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.03.01 #
 ******************************************************************************/
int html_pack_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, sprint_t *sp)
{
    int ret = 0, depth = 0;
    html_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth)
    {
        log_error(html->log, "Stack depth must empty. depth:[%d]", depth);
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 将要处理的节点压栈 */
        node->temp = node->firstchild;
        ret = stack_push(stack, node);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack push failed!");
            return HTML_ERR_STACK;
        }
        
        /* 2. 打印节点名 */
        depth = stack_depth(stack);
        
        html_pack_name(sp, node);
        
        /* 3. 打印属性节点 */
        if (html_has_attr(node))
        {
            html_pack_attr(sp, node);
        }
        
        /* 4. 打印节点值 */
        html_pack_value(sp, node);
        
        /* 5. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        node = html_pack_next(html, stack, node, sp);

    }while (NULL != node);

    if (!stack_isempty(stack))
    {
        log_error(html->log, "Stack is not empty!");
        return HTML_ERR_STACK;
    }
    return HTML_OK;
}

/******************************************************************************
 **                                                                          **
 **                              打印成有层次结构                            **
 **                                 (内存中)                                 **
 ******************************************************************************/
/* 打印节点名(注: HTML有层次格式) */
#define html_sprint_name(sp, node, depth) \
{ \
    while (depth > 1) \
    {   \
        sprintf(sp->ptr, "\t"); sp->ptr++;\
        depth--; \
    } \
    sprintf(sp->ptr, "<%s", node->name);\
    sp->ptr += strlen(sp->ptr);\
}

/* 打印属性节点(注: HTML有层次格式) */
#define html_sprint_attr(sp, node) \
{ \
    while (NULL != node->temp) \
    { \
        if (html_is_attr(node->temp)) \
        { \
            sprintf(sp->ptr, " %s=\"%s\"", node->temp->name, node->temp->value); \
            sp->ptr += strlen(sp->ptr); \
            node->temp = node->temp->next; \
            continue; \
        } \
        break; \
    } \
}

/* 打印节点值(注: HTML有层次格式) */
#define html_sprint_value(sp, node) \
{ \
    if (html_has_value(node)) \
    { \
        if (html_has_child(node))  /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">%s\n", node->value); \
        } \
        else \
        { \
            sprintf(sp->ptr, ">%s</%s>\n", node->value, node->name); \
        } \
        sp->ptr += strlen(sp->ptr); \
    } \
    else \
    { \
        if (NULL != node->temp)   /* 此时temp指向node的孩子节点 或 NULL */ \
        { \
            sprintf(sp->ptr, ">\n"); \
            sp->ptr += 2; \
        } \
        else \
        { \
            sprintf(sp->ptr, "/>\n"); \
            sp->ptr += 3; \
        } \
    } \
}

/******************************************************************************
 **函数名称: html_sprint_next
 **功    能: 选择下一个处理的节点(注: HTML有层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     sp: 缓存指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
static html_node_t *html_sprint_next(
        html_tree_t *html, Stack_t *stack, html_node_t *node, sprint_t *sp)
{
    int ret = 0, depth = 0, level = 0;
    html_node_t *top = NULL, *child = NULL;

    if (NULL != node->temp)      /* 首先: 处理孩子节点: 选出下一个孩子节点 */
    {
        child = node->temp;
        node->temp = child->next;
        node = child;
        return node;
    }
    else                        /* 再次: 处理其兄弟节点: 选出下一个兄弟节点 */
    {
        /* 1. 弹出已经处理完成的节点 */
        top = stack_gettop(stack);
        if (html_has_child(top))
        {
            depth = stack_depth(stack);
            level = depth - 1;
            while (level > 1)
            {
                sprintf(sp->ptr, "\t");
                sp->ptr++;
                level--;
            }
            sprintf(sp->ptr, "</%s>\n", top->name);
            sp->ptr += strlen(sp->ptr);
        }
        
        ret = stack_pop(stack);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack pop failed!");
            return NULL;
        }
        
        if (stack_isempty(stack))
        {
            log_error(html->log, "Compelte fprint!");
            return NULL;
        }
        
        /* 2. 处理其下一个兄弟节点 */
        node = top->next;
        while (NULL == node)     /* 所有兄弟节点已经处理完成，说明父亲节点也处理完成 */
        {
            /* 3. 父亲节点出栈 */
            top = stack_gettop(stack);
            ret = stack_pop(stack);
            if (HTML_OK != ret)
            {
                log_error(html->log, "Stack pop failed!");
                return NULL;
            }
        
            /* 4. 打印父亲节点结束标志 */
            if (html_has_child(top))
            {
                depth = stack_depth(stack);
                level = depth + 1;
                while (level > 1)
                {
                    sprintf(sp->ptr, "\t");
                    sp->ptr++;
                    level--;
                }
                sprintf(sp->ptr, "</%s>\n", top->name);
                sp->ptr += strlen(sp->ptr);
            }
            
            if (stack_isempty(stack))
            {
                return NULL;    /* 处理完成 */
            }

            /* 5. 选择父亲的兄弟节点 */
            node = top->next;
        }
    }    
    
    return node;
}

/******************************************************************************
 **函数名称: html_sprint_tree
 **功    能: 将栈中HTML节点的相关信息打印至缓存(注: HTML有层次格式)
 **输入参数:
 **     root: 根节点
 **     stack: 栈
 **     sp: 缓存指针
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **     注意:1. 刚开始时，栈的深度必须为1.
 **          2. 此时temp用来记录正在处理孩子节点
 **          3. 不打印root的兄弟节点
 **作    者: # Qifeng.zou # 2013.02.27 #
 ******************************************************************************/
int html_sprint_tree(html_tree_t *html, html_node_t *root, Stack_t *stack, sprint_t *sp)
{
    int ret = 0, depth = 0;
    html_node_t *node = root;

    depth = stack_depth(stack);
    if (0 != depth)
    {
        log_error(html->log, "Stack depth must empty. depth:[%d]", depth);
        return HTML_ERR_STACK;
    }

    do
    {
        /* 1. 将要处理的节点压栈 */
        node->temp = node->firstchild;
        ret = stack_push(stack, node);
        if (HTML_OK != ret)
        {
            log_error(html->log, "Stack push failed!");
            return HTML_ERR_STACK;
        }
        
        /* 2. 打印节点名 */
        depth = stack_depth(stack);
        
        html_sprint_name(sp, node, depth);
        
        /* 3. 打印属性节点 */
        if (html_has_attr(node))
        {
            html_sprint_attr(sp, node);
        }
        
        /* 4. 打印节点值 */
        html_sprint_value(sp, node);
        
        /* 5. 选择下一个处理的节点: 从父亲节点、兄弟节点、孩子节点中 */
        node = html_sprint_next(html, stack, node, sp);

    }while (NULL != node);

    if (!stack_isempty(stack))
    {
        log_error(html->log, "Stack is not empty!");
        return HTML_ERR_STACK;
    }

    return HTML_OK;
}
