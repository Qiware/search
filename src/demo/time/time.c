/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: time.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Fri 19 Jun 2015 05:11:41 PM CST #
 ******************************************************************************/
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
int main(void)
{
    int idx = 0;
    struct timespec time_start={0, 0},time_end={0, 0};

    for (idx=0; idx<1000000; ++idx)
    {
        ++idx;
    #if 0
        clock_gettime(CLOCK_REALTIME, &time_start);
        clock_gettime(CLOCK_REALTIME, &time_end);
    #else
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        clock_gettime(CLOCK_MONOTONIC, &time_end);
    #endif

        printf("idx:%d start time %llus,%llu ns\n", idx, time_start.tv_sec, time_start.tv_nsec);
        printf("idx:%d endtime %llus,%llu ns\n", idx, time_end.tv_sec, time_end.tv_nsec);
        printf("idx:%d duration:%llus %lluns\n", idx, time_end.tv_sec-time_start.tv_sec, time_end.tv_nsec-time_start.tv_nsec);
    }
    return 0;
}
