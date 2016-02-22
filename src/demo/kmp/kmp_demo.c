/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: kmp_demo.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 27 May 2015 07:15:01 PM CST #
 ******************************************************************************/
#include "kmp.h"

int main(int argc, char *argv[])
{
    kmp_t *kmp;
    char *pat;
    char input[256];

    if (2 != argc) {
        fprintf(stderr, "Arugment number isn't right!");
        return -1;
    }

    pat = argv[1];
    kmp = kmp_creat(pat, strlen(pat));
    if (NULL == kmp) {
        return -1;
    }

    kmp_print(kmp);

    while (1) {
        fprintf(stdout, "Pattern: %s\n", kmp->p);
        fprintf(stdout, "Input string:");
        scanf(" %s", input);
        fprintf(stdout, "idx:%d", kmp_match(kmp, input, strlen(input)));
    }

    kmp_destroy(kmp);

    return 0;
}
