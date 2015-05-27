#include "kmp.h"

/******************************************************************************
 **函数名称: kmp_next
 **功    能: 构建next数组
 **输入参数:$
 **     pattern: 模式串
 **     len: 模式串长度
 **输出参数: NONE
 **返    回: next数组
 **实现描述:$
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
static int *kmp_next(char *pattern, int len)
{
    int *next;
    int i = 0, j = -1;

    next = (int *)calloc(1, len);
    if (NULL == next)
    {
        return NULL;
    }

    next[0] = -1;
    while (i < len)
    {
        while (-1 != j && pattern[i] == pattern[j])
        {
            j = next[j];
        }
        ++i;
        ++j;
        next[i] = j;
    }

    return next;
}

/******************************************************************************
 **函数名称: kmp_creat
 **功    能: 构建KMP对象
 **输入参数:$
 **     str: 模式串
 **     len: 模式串长度
 **输出参数: NONE
 **返    回: KMP对象
 **实现描述:$
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
kmp_t *kmp_creat(char *pattern, int len)
{
    kmp_t *kmp;

    kmp = (kmp_t *)calloc(1, sizeof(kmp_t));
    if (NULL == kmp)
    {
        return NULL;
    }

    kmp->next = kmp_next(pattern, len);
    if (NULL == kmp->next)
    {
        free(kmp);
        return NULL;
    }

    return kmp;
}

/******************************************************************************
 **函数名称: kmp_match
 **功    能: 通过KMP算法匹配指定对象
 **输入参数:$
 **     kmp: KMP对象
 **     data: 匹配数据
 **     len: 数据长度
 **输出参数: NONE
 **返    回: 返回匹配索引值
 **实现描述:$
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.27 #
 ******************************************************************************/
int kmp_match(kmp_t *kmp, const char *data, int len)
{
    int i = 0, j = 0;

    while ((i < len-kmp->len+1) && (j < kmp->len))
    {
        if ((-1 == j) || (data[i] == kmp->pattern[j]))
        {
            ++i;
            ++j;
        }
        else
        {
            j = kmp->next[j];
        }
    }

    if(j >= kmp->len) { return i - kmp->len; }

    return -1;
}
