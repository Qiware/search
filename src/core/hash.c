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
