#include <signal.h>
#include <sys/time.h>

#include "mesg.h"
#include "syscall.h"
#include "sdsd_cli.h"
#include "sdsd_ssvr.h"

#define __SDTP_DEBUG_SEND__

/******************************************************************************
 **函数名称: sdtp_send_debug 
 **功    能: 发送端调试
 **输入参数: 
 **     cli: 上下文信息
 **输出参数: NONE
 **返    回: 0:Success !0:Failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.03.26 #
 ******************************************************************************/
#define LOOP        (100000)
#define USLEEP      (10)
#define SIZE        (4096)

int sdtp_send_debug(sdsd_cli_t *cli, int secs)
{
    size_t idx = 0;
    double sleep2 = 0;
    struct timeval stime, etime;
    int total = 0, fails = 0;
    char data[SIZE];
    mesg_search_word_body_t *body;

    for (;;)
    {
        gettimeofday(&stime, NULL);
        sleep2 = 0;
        fails = 0;
        total = 0;
        for (idx=0; idx<LOOP; idx++)
        {
            body = (mesg_search_word_body_t *)data;

            snprintf(body->words, sizeof(body->words), "%s", "BAIDU");

            if (sdsd_cli_send(cli, MSG_SEARCH_WORD_REQ, body, sizeof(mesg_search_word_body_t)))
            {
                idx--;
                usleep(2);
                sleep2 += USLEEP*1000000;
                ++fails;
                continue;
            }

            total++;
        }

        gettimeofday(&etime, NULL);
        if (etime.tv_usec < stime.tv_usec)
        {
            etime.tv_sec--;
            etime.tv_usec += 1000000;
        }

        fprintf(stderr, "%s() %s:%d\n"
                "\tstime:%ld.%06ld etime:%ld.%06ld spend:%ld.%06ld\n"
                "\tTotal:%d fails:%d\n",
                __func__, __FILE__, __LINE__,
                stime.tv_sec, stime.tv_usec,
                etime.tv_sec, etime.tv_usec,
                etime.tv_sec - stime.tv_sec,
                etime.tv_usec - stime.tv_usec,
                total, fails);

    }

    pause();

    return 0;
}

static void sdtp_setup_conf(sdsd_conf_t *conf, int port)
{
    conf->nodeid = 1;
    snprintf(conf->name, sizeof(conf->name), "SDTP-SEND");

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "qifeng");
    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "111111");

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "127.0.0.1");

    conf->port = port;
    conf->send_thd_num = 1;
    conf->send_buff_size = 5 * MB;
    conf->recv_buff_size = 2 * MB;

    snprintf(conf->sendq.name, sizeof(conf->sendq.name), "../temp/sdtp/sdtp-ssvr.key");
    conf->sendq.size = 4096;
    conf->sendq.count = 2048;
}

int main(int argc, const char *argv[])
{
    int port;
    log_cycle_t *log;
    sdsd_cntx_t *ctx;
    sdsd_conf_t conf;

    if (2 != argc)
    {
        fprintf(stderr, "Didn't special port!");
        return -1;
    }

    memset(&conf, 0, sizeof(conf));

    signal(SIGPIPE, SIG_IGN);

    nice(-20);

    port = atoi(argv[1]);
    sdtp_setup_conf(&conf, port);

    log = log_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.log");
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    ctx = sdsd_init(&conf, log);
    if (NULL == ctx) 
    {
        fprintf(stderr, "Initialize send-server failed!");
        return -1;
    }

    if (sdsd_start(ctx))
    {
        fprintf(stderr, "Start up send-server failed!");
        return -1;
    }

#if defined(__SDTP_DEBUG_SEND__)
    sdsd_cli_t *cli;
    sdsd_cli_t *cli2;
 
    Sleep(5);
    cli = sdsd_cli_init(&conf, 0, log);
    if (NULL == cli)
    {
        fprintf(stderr, "Initialize send module failed!");
        return -1;
    }

    cli2 = sdsd_cli_init(&conf, 1, log);
    if (NULL == cli2)
    {
        fprintf(stderr, "Initialize send module failed!");
        return -1;
    }

    Sleep(5);

    sdtp_send_debug(cli, 5);
#endif /*__SDTP_DEBUG_SEND__*/

    while (1) { pause(); }

    fprintf(stderr, "Exit send server!");

    return 0;
}
