#include "kmp.h"

/******************************************************************************
 **函数名称: kmp_gen_next
 **功    能: 构建next数组
 **输入参数:
 **     p: 模式串
 **     len: 模式串长度
 **输出参数: NONE
 **返    回: next数组
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
static int *kmp_gen_next(char *p, int len)
{
    int *next;
    int k = -1, j = 0;

    next = (int *)calloc(1, len);
    if (NULL == next)
    {
        return NULL;
    }

    next[0] = -1;
    while (j < len - 1)
    {
        /* p[k]表示前缀, p[j]表示后缀 */
        if ((-1 == k) || (p[j] == p[k]))
        {
            ++k;
            ++j;
            next[j] = k;
        }
        else
        {
            k = next[k];
        }
    }

    return next;
}

/******************************************************************************
 **函数名称: kmp_creat
 **功    能: 构建KMp对象
 **输入参数:
 **     str: 模式串
 **     len: 模式串长度
 **输出参数: NONE
 **返    回: KMp对象
 **实现描述: 创建对象，并构建next数组
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
kmp_t *kmp_creat(char *p, int len)
{
    kmp_t *kmp;

    kmp = (kmp_t *)calloc(1, sizeof(kmp_t));
    if (NULL == kmp)
    {
        return NULL;
    }

    kmp->next = kmp_gen_next(p, len);
    if (NULL == kmp->next)
    {
        free(kmp);
        return NULL;
    }

    kmp->len = len;
    kmp->p = p;

    return kmp;
}

/******************************************************************************
 **函数名称: kmp_match
 **功    能: 通过KMp算法匹配指定对象
 **输入参数:
 **     kmp: KMp对象
 **     s: 主串
 **     len: 主串长度
 **输出参数: NONE
 **返    回: 返回匹配索引值
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
int kmp_match(kmp_t *kmp, const char *s, int len)
{
    int i = 0, j = 0;

    while (i < len && j < kmp->len)
    {
        /* ① 如果j = -1, 或者当前字符匹配成功(即s[i] == p[j]), 都令i++，j++ */
        if (j == -1 || s[i] == kmp->p[j])
        {
            i++;
            j++;
        }
        else
        {
            /* ② 如果j != -1, 且当前字符匹配失败(即s[i] != p[j]),
             * 则令i不变, j = next[j] (注: next[j]即为j所对应的next值) */
            j = kmp->next[j];
        }
    }

    if (j == kmp->len)
    {
        return i - j;
    }

    return -1;
}

/* 释放内存空间 */
void kmp_destroy(kmp_t *kmp)
{
    free(kmp->next);
    free(kmp);
}
