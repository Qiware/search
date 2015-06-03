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
