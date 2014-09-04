#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "hash.h"

/******************************************************************************
 ** Name : hash_time33
 ** Desc : TIME33哈希算法
 ** Input: 
 **     str: string
 ** Output: NONE
 ** Return: 哈希值
 ** Proc :
 ** Note :
 **     该算法特点: 算法简单、性能高效、散列效果佳.
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
uint32_t hash_time33(const char *str)
{
    const char *p = str;
    uint32_t hash = 5381;

    while (*p)
    {
        hash += (hash << 5) + (*p++);
    }

    return (hash & 0x7FFFFFFF);
}

/******************************************************************************
 ** Name : hash_time33_ex
 ** Desc : TIME33哈希算法
 ** Input: 
 **     addr: 内存地址
 **     len: 长度
 ** Output: NONE
 ** Return: 哈希值
 ** Proc :
 ** Note :
 **     该算法特点: 算法简单、性能高效、散列效果佳.
 ** Author: # Qifeng.zou # 2014.04.18 #
 ******************************************************************************/
uint32_t hash_time33_ex(const char *addr, int len)
{
    uint32_t hash = 5381;
    const char *p = addr;

    while (len-- > 0)
    {
        hash += (hash << 5) + (*p++);
    }

    return (hash & 0x7FFFFFFF);
}
