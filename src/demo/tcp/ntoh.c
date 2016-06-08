/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: ntol.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年04月25日 星期一 06时39分09秒 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

int main(int argc, void *argv[])
{
    char ch;
    int digit;

    if (3 != argc) {
        fprintf(stderr, "Parmaters isn't right!");
        return -1;
    }

    digit = atoi(argv[1]);
    ch = ((char *)argv[2])[0];

    switch (ch) {
        case 'l':
        case 'L':
            digit = htonl(digit);
            break;
        case 's':
        case 'S':
            digit = htons(digit);
            break;
    }
    fprintf(stderr, "dight:%d/0x%X!", digit, digit);
    return 0;
}
