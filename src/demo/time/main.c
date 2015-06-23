/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: main.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Fri 19 Jun 2015 06:18:49 PM CST #
 ******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
int main(void)
{
    int i = 100;
    struct timeval tv[100];
    while(i --)
    {
        gettimeofday(&tv[i], 0);
        usleep(0);
    }
    for(i = 99; i >=0; i--)
    {
        printf("%lu\n", tv[i].tv_sec*1000 + tv[i].tv_usec);
    }
    return 0;
}
