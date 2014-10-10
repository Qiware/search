#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <hiredis/hiredis.h>

#define REDIS_SERVER_IP_ADDR    "127.0.0.1"
#define REDIS_SERVER_PORT       (6379)

void redis_test(void)
{
    int i;
    redisReply *r;
    redisContext *ctx;
    struct timeval tv;

    tv.tv_sec = 30;
    tv.tv_usec = 0;

    /* 1. 连接Redis服务器
     * 带有超时的方式链接Redis服务器, 同时获取与Redis连接的上下文对象.
     * 该对象将用于其后所有与Redis操作的函数. */
    ctx = redisConnectWithTimeout(REDIS_SERVER_IP_ADDR, REDIS_SERVER_PORT, tv);
    if (ctx->err)
    {
        fprintf(stderr, "Connect redis server failed! ip:%s port:%d",
                REDIS_SERVER_IP_ADDR, REDIS_SERVER_PORT);
        goto ERROR;
    }

    /* 2. 设置KEY-VALUE对
     * 需要注意的是, 如果返回的对象是NULL, 则表示客户端和服务器之间出现严重错误, 
     * 必须重新链接. 这里只是举例说明, 简便起见, 后面的命令就不再做这样的判断了. */
    const char *command1 = "set stest1 value1";

    r = (redisReply *)redisCommand(ctx, command1);
    if (NULL == r)
    {
        fprintf(stderr, "Execute [%s] failed!", command1);
        goto ERROR;
    }

    /* 不同的Redis命令返回的数据类型不同, 在获取之前需要先判断它的实际类型.
     * 至于各种命令的返回值信息, 可以参考Redis的官方文档, 或者查看该系列博客的前几篇
     * 有关Redis各种数据类型的博客.:)
     * 字符串类型的set命令的返回值的类型是REDIS_REPLY_STATUS, 然后只有当返回信息是"OK"
     * 时, 才表示该命令执行成功.后面的例子以此类推, 就不再过多赘述了. */
    if (REDIS_REPLY_STATUS != r->type
        || 0 != strcasecmp(r->str, "OK"))
    {
        fprintf(stderr, "Execute [%s] failed!\n", command1);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] Success!\n", command1);

    /* 由于后面重复使用该变量, 所以需要提前释放, 否则内存泄漏. */
    freeReplyObject(r);

    /* 3. 获取KEY的VALUE长度 */
    const char *command2 = "strlen stest1";

    r = (redisReply *)redisCommand(ctx, command2);
    if (REDIS_REPLY_INTEGER != r->type)
    {
        fprintf(stderr, "Execute [%s] failed!\n", command2);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] Success! Length:%lld\n", command2, r->integer);

    freeReplyObject(r);

    /* 4. 获取KEY的VALUE值 */
    const char *command3 = "get stest1";

    r = (redisReply *)redisCommand(ctx, command3);
    if (REDIS_REPLY_STRING != r->type)
    {
        fprintf(stderr, "Execute [%s] failed!\n", command3);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] Success. Value:%s\n", command3, r->str);

    freeReplyObject(r);

    /* 5. 获取KEY的VALUE值 */
    const char *command4 = "get stest2";

    r = (redisReply *)redisCommand(ctx, command4);
    /* 这里需要先说明一下, 由于stest2键并不存在, 因此Redis会返回空结果, 
     * 这里只是为了演示. */
    if (REDIS_REPLY_NIL != r->type)
    {
        fprintf(stderr, "Execute [%s] failed!\n", command4);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] Success!\n", command4);

    freeReplyObject(r);

    /* 6. 获取多个KEY的VALUE值 */
    const char *command5 = "mget stest1 stest2";

    r = (redisReply *)redisCommand(ctx, command5);
    /* 不论stest2存在与否, Redis都会给出结果, 只是第二个值为nil.
     * 由于有多个值返回, 因为返回应答的类型是数组类型. */
    if (REDIS_REPLY_ARRAY != r->type)
    {
        fprintf(stderr, "Execute [%s] failed!\n", command5);
        freeReplyObject(r);
        /* r->elements表示子元素的数量, 不管请求的key是否存在, 
         * 该值都等于请求是键的数量. */
        assert(2 == r->elements);
        goto ERROR;
    }

    for (i = 0; i < r->elements; ++i)
    {
        redisReply* childReply = r->element[i];
        /* 之前已经介绍过, get命令返回的数据类型是string.
         * 对于不存在key的返回值, 其类型为REDIS_REPLY_NIL. */
        if (REDIS_REPLY_STRING == childReply->type)
        {
            printf("Value[%d]: %s\n", i, childReply->str);
        }
    }

    printf("Execute [%s] Success!\n", command5);

    /* 对于每一个子应答, 无需使用者单独释放, 只需释放最外部的redisReply即可. */
    freeReplyObject(r);

    /* 7. 测试异步IO通信 */
    printf("Begin to test pipeline.\n");

    /* 该命令只是将待发送的命令写入到上下文对象的输出缓冲区中, 直到调用后面的
     * redisGetReply命令才会批量将缓冲区中的命令写出到Redis服务器.这样可以
     * 有效的减少客户端与服务器之间的同步等候时间, 以及网络IO引起的延迟.
     * 至于管线的具体性能优势, 可以考虑该系列博客中的管线主题. */
    if (REDIS_OK != redisAppendCommand(ctx, command1)
            || REDIS_OK != redisAppendCommand(ctx, command2)
            || REDIS_OK != redisAppendCommand(ctx, command3)
            || REDIS_OK != redisAppendCommand(ctx, command4)
            || REDIS_OK != redisAppendCommand(ctx, command5))
    {
        fprintf(stderr, "Execute append command failed!\n");
        goto ERROR;
    }

    /* 对pipeline返回结果的处理方式, 和前面代码的处理方式完全一直, 
     * 这里就不再重复给出了. */
    if (REDIS_OK != redisGetReply(ctx, (void **)&r))
    {
        fprintf(stderr, "Execute [%s] with pipeline failed!\n", command1);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] with pipeline success.\n", command1);

    freeReplyObject(r);

    if (REDIS_OK != redisGetReply(ctx, (void **)&r))
    {
        fprintf(stderr, "Execute [%s] with pipeline failed!\n", command2);
        freeReplyObject(r);
        goto ERROR;
    }

    printf("Execute [%s] with pipeline success!\n", command2);

    freeReplyObject(r);

    if (REDIS_OK != redisGetReply(ctx, (void **)&r))
    {
        fprintf(stderr, "Execute [%s] with pipeline failed!.\n", command3);
        freeReplyObject(r);
        goto ERROR;
    }

    freeReplyObject(r);
    printf("Execute [%s] with pipeline success!\n", command3);

    if (REDIS_OK != redisGetReply(ctx, (void **)&r))
    {
        fprintf(stderr, "Execute [%s] with pipeline failed!\n", command4);
        freeReplyObject(r);
        goto ERROR;
    }

    freeReplyObject(r);

    printf("Execute [%s] with pipeline success!\n", command4);

    if (REDIS_OK != redisGetReply(ctx, (void **)&r))
    {
        fprintf(stderr, "Execute [%s] with pipeline failed!\n", command5);
        freeReplyObject(r);
        goto ERROR;
    }

    freeReplyObject(r);
    printf("Execute [%s] with pipeline success!\n", command5);

    //由于所有通过pipeline提交的命令结果均已为返回, 如果此时继续调用redisGetReply, 
    //将会导致该函数阻塞并挂起当前线程, 直到有新的通过管线提交的命令结果返回.
    //最后不要忘记在退出前释放当前连接的上下文对象.
ERROR:
    redisFree(ctx);
    return;
}

int main(void)
{
    redis_test();
    return 0;
}
