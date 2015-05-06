/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: mysql_connect.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Mon 04 May 2015 01:09:39 PM CST #
 ******************************************************************************/

#include <mysql.h>

int main(void)
{
    MYSQL *mysql;

    mysql = mysql_init(NULL);
    if (NULL == mysql)
    {
        fprintf(stderr, "Initialize mysql failed!");
        return -1;
    }

    if (!mysql_real_connect(mysql, "localhost", "qifeng", "1111111", "crwl", 8888, NULL, 0))
    {
        fprintf(stderr, "Initialize mysql failed!");
        mysql_close(mysql);
        return -1;
    }

    return 0;
}
