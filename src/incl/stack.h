/******************************************************************************
 ** Copyright(C) 2013-2014 Qiware technology Co., Ltd
 **
 ** 文件名: stack.h
 ** 版本号: 1.0
 ** 描  述: 通用栈的结构定义和函数声明
 ** 作  者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
#if !defined(__STACK_H__)
#define __STACK_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

/* 通用栈定义 */
typedef struct
{
    void **base;    /* 栈基地址 */
    void **top;     /* 栈顶地址 */
    int max;        /* 栈的大小 */
} Stack_t;

/******************************************************************************
 **函数名称: stack_init
 **功    能: 栈初始化
 **输入参数:
 **      stack: 栈
 **      max: 栈的大小
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static inline int stack_init(Stack_t *stack, int max)
{
    stack->base = (void **)calloc(max, sizeof(void *));
    if (NULL == stack->base) {
        return -1;
    }
    stack->top= stack->base;
    stack->max = max;

    return 0;
}

/******************************************************************************
 **函数名称: stack_destroy
 **功    能: 释放栈
 **输入参数:
 **      stack: 被释放的栈
 **输出参数:
 **返    回: 0: 成功  !0: 失败
 **实现描述: 
 **注意事项: 
 **      在此处并不释放xml_stack_node_t中name, value的内存空间。
 **      因为这些空间已交由xml树托管。释放xml树时，自然会释放这些空间。
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static inline int stack_destroy(Stack_t *stack)
{
    free(stack->base);
    stack->base = NULL;
    stack->top = NULL;
    stack->max = 0;

    return 0;
}

/******************************************************************************
 **函数名称: stack_push
 **功    能: 入栈
 **输入参数:
 **      stack: 栈
 **输出参数:
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static inline int stack_push(Stack_t *stack, void *node)
{
    if ((stack->top - stack->base) >= stack->max) {
        return -1;
    }

    *(stack->top) = node;
    stack->top++;

    return 0;
}

/******************************************************************************
 **函数名称: stack_pop
 **功    能: 出栈
 **输入参数:
 **      stack: 栈
 **输出参数:
 **返    回: 栈顶节点地址
 **实现描述: 
 **注意事项: 
 **      在此只负责将节点弹出栈，并不负责内存空间的释放
 **作    者: # Qifeng.zou # 2013.02.05 #
 ******************************************************************************/
static inline void *stack_pop(Stack_t *stack)
{
    if (stack->base == stack->top) {
        return NULL;
    }

    stack->top--;
    return *(stack->top);
}

#define stack_isempty(stack) (((stack)->base == (stack)->top)? true : false) /* 栈是否为空 */
#define stack_gettop(stack) (((stack)->base == (stack)->top)? NULL: *((stack)->top-1)) /* 取栈顶元素 */
#define stack_depth(stack) ((stack)->top - (stack)->base)  /* 栈当前深度 */
#define stack_maxdepth(stack) ((stack)->max)   /* 栈最大深度 */

#endif /*__STACK_H__*/
