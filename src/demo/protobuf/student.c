/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: student.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年09月11日 星期日 22时25分33秒 #
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "student.pb-c.h"

int main(int argc, char *argv[])
{
    int len;
    void *buf;
    CStudent student = CSTUDENT__INIT;

    if (argc < 2) {
        fprintf(stderr, "Miss paramter!\n");
        return 0;
    }

    student.name = argv[1];
    if (3 == argc) {
        student.has_age = 1;
        student.age = atoi(argv[2]);
    }

    len = cstudent__get_packed_size(&student);

    buf = (void *)calloc(1, len);
    if (NULL == buf) {
        return -1;
    }

    cstudent__pack(&student, buf);

    free(buf);

    return 0;
}
