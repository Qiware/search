#include <signal.h>
#include <sys/time.h>

#include "syscall.h"
#include "sdtp_cli.h"
#include "sdtp_ssvr.h"

#define SDTP_LOG_LEVEL   LOG_LEVEL_TRACE
#define SDTP_LOG_SIZE    (100*1024*1024)
#define SDTP_LOG_COUNT   (10)
#define SDTP_LOG_PATH    ("./sdtp_snd.log")
#define SDTP_LOG_TYPE    (LOG_TO_FILE)


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

int sdtp_send_debug(sdtp_cli_t *cli, int secs)
{
    size_t loop = 0, idx = 0;
    double sleep2 = 0;
    struct timeval stime, etime;
    int total = 0, fails = 0;
    char data[SIZE] = {0};

    memset(data, 'A', sizeof(data)-2);
    data[SIZE-1] = '\n';

    loop = 1000000;
    Sleep(2);

    for (;;)
    {
        gettimeofday(&stime, NULL);
        sleep2 = 0;
        fails = 0;
        total = 0;
        for (idx=0; idx<loop; idx++)
        {
            if (sdtp_cli_send(cli, idx%3, data, rand()%SIZE + 1))
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

static void sdtp_setup_conf(sdtp_ssvr_conf_t *conf)
{
    snprintf(conf->name, sizeof(conf->name), "SDTP-SEND");
    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "127.0.0.1");
    conf->port = 54321;
    conf->snd_thd_num = 1;
    conf->send_buff_size = 5 * MB;
    conf->recv_buff_size = 2 * MB;

    snprintf(conf->qcf.name, sizeof(conf->qcf.name), "../temp/sdtp/sdtp-ssvr.key");
    conf->qcf.size = 4096;
    conf->qcf.count = 10000;
}

int main(int argc, const char *argv[])
{
    int sleep = 5;
    log_cycle_t *log;
    sdtp_ssvr_cntx_t *ctx;
    sdtp_cli_t *cli;
    sdtp_cli_t *cli2;
    sdtp_ssvr_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    signal(SIGPIPE, SIG_IGN);

    nice(-20);

    sdtp_setup_conf(&conf);

    log2_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.log2");
    log = log_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.log");

    ctx = sdtp_ssvr_startup(&conf, log);
    if (NULL == ctx) 
    {
        fprintf(stderr, "Start up send-server failed!");
        return -1;
    }

    Sleep(5);
    cli = sdtp_cli_init(&conf, 0, log);
    if (NULL == cli)
    {
        fprintf(stderr, "Initialize send module failed!");
        return -1;
    }

    cli2 = sdtp_cli_init(&conf, 1, log);
    if (NULL == cli2)
    {
        fprintf(stderr, "Initialize send module failed!");
        return -1;
    }

    Sleep(5);

    sdtp_send_debug(cli, sleep);

    while (1) { pause(); }

    fprintf(stderr, "Exit send server!");

    return 0;
}
