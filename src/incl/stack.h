/******************************************************************************
 ** Coypright(C) 2013-2014 Xundao technology Co., Ltd
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

/* 栈的优化开关 */
#define __STACK_OPTIMIZE__

/* 通用栈定义 */
typedef struct
{
    void **base;    /* 栈基地址 */
    void **top;     /* 栈顶地址 */
    int size;       /* 栈的大小 */
}Stack_t;

extern int stack_init(Stack_t *stack, int size);

#if defined(__STACK_OPTIMIZE__)
/* 释放栈 */
#define stack_destroy(stack) \
( \
    free((stack)->base), \
    (stack)->base = NULL, \
    (stack)->top = NULL, \
    (stack)->size = 0 \
)

/* 压栈 */
#define stack_push(stack, node) \
( \
    (((stack)->top-(stack)->base) >= (stack)->size)? (-1): \
    ( \
        *((stack)->top) = (node), \
        (stack)->top++, 0 \
    ) \
)

/* 入栈 */
#define stack_pop(stack) \
(   \
    ((stack)->base == (stack)->top)? (-1): \
    ( \
        (stack)->top--, \
        *((stack)->top) = NULL, 0 \
    ) \
)
#else   /*__STACK_OPTIMIZE__*/
void stack_destroy(Stack_t *stack);
int stack_push(Stack_t *stack, void *node);
int stack_pop(Stack_t * stack);
#endif /*__STACK_OPTIMIZE__*/

#define stack_isempty(stack) (((stack)->base == (stack)->top)? true : false) /* 栈是否为空 */
#define stack_gettop(stack) (((stack)->base == (stack)->top)? NULL: *((stack)->top-1)) /* 取栈顶元素 */
#define stack_depth(stack) ((stack)->top - (stack)->base)  /* 栈当前深度 */
#define stack_maxdepth(stack) ((stack)->size)   /* 栈最大深度 */

#endif /*__STACK_H__*/
