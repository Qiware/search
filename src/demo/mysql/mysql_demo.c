#include <stdio.h>
#include <mysql.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MYSQL_SERVER    "localhost"
#define MYSQL_USER      "root"
#define MYSQL_PASSWD    "Zqf198703"
#define MYSQL_DATABASE  "school"

static int sql_exec_and_print(MYSQL *conn, const char *sql);

int main(void)
{
    MYSQL *conn;

    /* > 初始化MYSQL对象 */
    conn = mysql_init(NULL);
    if (NULL == conn) {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* > 连接MYSQL数据库 */
    if (!mysql_real_connect(
            conn, MYSQL_SERVER, MYSQL_USER,
            MYSQL_PASSWD, MYSQL_DATABASE, 0, NULL, 0))
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return -1;
    }

    /* > 执行MYSQL语句 */
    sql_exec_and_print(conn, "SHOW TABLES");
    sql_exec_and_print(conn, "SELECT * FROM city");
    sql_exec_and_print(conn, "CALL query_city(1)");

    /* > 关闭MYSQL连接 */
    mysql_close(conn);
    return 0;
}

/* 执行并打印返回结果 */
static int sql_exec_and_print(MYSQL *conn, const char *sql)
{
    MYSQL_ROW row;
    MYSQL_RES *res;
    unsigned int cols, col, idx;

    /* > 执行SQL语句 */
    if (mysql_query(conn, sql)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return -1;
    }

    /* > 获取返回结果 */
    res = mysql_use_result(conn);
    cols = mysql_num_fields(res);

    printf("Query result: [%s]\n", sql);
    idx = 0;
    while (NULL != (row = mysql_fetch_row(res))) {
        printf("[%03d] ", ++idx);
        for (col=0; col<cols; ++col) {
            printf("[%s] ", row[col]);
        }
        printf("\n");
    }

    /* > 释放查询结果 */
    mysql_free_result(res);

    return 0;
}
