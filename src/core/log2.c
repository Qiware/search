/*******************************************************************************
 ** 模  块: 同步日志模块
 ** 说  明: 
 **     主要负责系统内部模块错误的跟踪处理.
 ** 注  意: 
 ** 作  者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
#include <sys/shm.h>
#include <sys/types.h>

#include "log.h"
#include "common.h"
#include "syscall.h"

#if defined(__XDO_DEBUG__)
log2_cycle_t g_log2 = {NULL, 0xFFFFFFFF, -1};
#else /*!__XDO_DEBUG__*/
log2_cycle_t g_log2 = {NULL, LOG_LEVEL_ERROR|LOG_LEVEL_FATAL, -1};
#endif /*!__XDO_DEBUG__*/

static int log2_write(log2_cycle_t *log, int level,
    const void *dump, int dumplen, const char *errmsg, const struct timeb *ctm);
static int log2_print_dump(log2_cycle_t *log, const void *dump, int dumplen);

/******************************************************************************
 **函数名称: log_get_level
 **功    能: 获取日志级别
 **输入参数: 
 **     level_str: 日志级别字串
 **输出参数: NONE
 **返    回: 日志级别
 **实现描述: 
 **注意事项: 
 **     当级别匹配失败时，默认情况下将开启所有日志级别.
 **作    者: # Qifeng.zou # 2014.09.03 #
 ******************************************************************************/
int log2_get_level(const char *level_str)
{
    if (!strcasecmp(level_str, LOG_LEVEL_FATAL_STR))
    {
        return LOG_LEVEL_FATAL;
    }

    if (!strcasecmp(level_str, LOG_LEVEL_ERROR_STR))
    {
        return LOG_LEVEL_ERROR;
    }

    if (!strcasecmp(level_str, LOG_LEVEL_WARN_STR))
    {
        return LOG_LEVEL_WARN;
    }

    if (!strcasecmp(level_str, LOG_LEVEL_INFO_STR))
    {
        return LOG_LEVEL_INFO;
    }

    if (!strcasecmp(level_str, LOG_LEVEL_DEBUG_STR))
    {
        return LOG_LEVEL_DEBUG;
    }

    if (!strcasecmp(level_str, LOG_LEVEL_TRACE_STR))
    {
        return LOG_LEVEL_TRACE;
    }

    return LOG_LEVEL_TOTAL;
}

/******************************************************************************
 **函数名称: log2_init
 **功    能: 初始化日志模块
 **输入参数: 
 **     level: 日志级别(LOG_LEVEL_TRACE ~ LOG_LEVEL_FATAL)
 **     path: 日志路径
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
int log2_init(const char *level, const char *path)
{
    log2_cycle_t *log2 = &g_log2;

    if (NULL != log2->fp)
    {
        return 0;
    }

    Mkdir2(path, DIR_MODE);

    log2->fp = fopen(path, "aw+");
    if (NULL == log2->fp)
    {
        return -1;
    }

    log2->level = log2_get_level(level);
    log2->pid = getpid();
    return 0;
}

/******************************************************************************
 **函数名称: log2_core
 **功    能: 日志核心调用
 **输入参数: 
 **     level: 日志级别(LOG_LEVEL_TRACE ~ LOG_LEVEL_FATAL)
 **     fname: 文件名
 **     lineno: 文件行号
 **     dump: 需打印的内存地址
 **     dumplen: 需打印的内存长度
 **     fmt: 格式化输出
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     1. 判断日志级别
 **     2. 获取系统时间
 **     3. 组合日志信息
 **     4. 日志同步处理
 **注意事项: 
 **     日志级别的判断在函数外进行判断
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
void log2_core(int level,
            const char *fname, int lineno,
            const void *dump, int dumplen,
            const char *fmt, ...)
{
    int len;
    va_list args;
    struct timeb ctm;
    char errmsg[LOG_MSG_MAX_LEN];

    va_start(args, fmt);
    len = snprintf(errmsg, sizeof(errmsg), "[%s][%d] ", fname, lineno);
    vsnprintf(errmsg + len, sizeof(errmsg) - len, fmt, args);
    va_end(args);

    ftime(&ctm);
    
    log2_write(&g_log2, level, dump, dumplen, errmsg, &ctm);
}

/******************************************************************************
 **函数名称: log2_destroy
 **功    能: 销毁日志模块
 **输入参数: NONE
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
void log2_destroy(void)
{
    fflush(g_log2.fp);
    fClose(g_log2.fp);
}

/******************************************************************************
 **函数名称: log2_write
 **功    能: 将日志信息写入缓存
 **输入参数: 
 **     fp: 文件指针
 **     level: 日志级别
 **     dump: 内存地址
 **     dumplen: 需打印的地址长度
 **     msg: 日志内容
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
static int log2_write(log2_cycle_t *log, int level,
    const void *dump, int dumplen,
    const char *errmsg, const struct timeb *ctm)
{
    struct tm loctm;

    localtime_r(&ctm->time, &loctm);      /* 获取当前系统时间 */

    switch (level)
    {
        case LOG_LEVEL_FATAL:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|FATAL %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_ERROR:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|ERROR %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_WARN:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|WARN %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_INFO:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|INFO %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_DEBUG:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|DEBUG %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        case LOG_LEVEL_TRACE:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|TRACE %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
        default:
        {
            /* @进程号|YYYYMMDD|HH:MM:SS.MM|级别提示 日志内容 */
            fprintf(log->fp, "@%d|%04d%02d%02d|%02d:%02d:%02d.%03d|OTHER %s\n",
                    log->pid, loctm.tm_year+1900, loctm.tm_mon+1, loctm.tm_mday,
                    loctm.tm_hour, loctm.tm_min, loctm.tm_sec,
                    ctm->millitm, errmsg);
            break;
        }
    } 

    /* 打印DUMP数据 */
    if((NULL != dump) && (dumplen > 0)) 
    {
        log2_print_dump(log, dump, dumplen);
    }

    return 0;
}

/******************************************************************************
 **函数名称: log2_print_dump
 **功    能: 以16进制打印日志信息
 **输入参数: 
 **     fp: 文件指针
 **     dump: 内存首地址
 **     dumplen: 打印长度
 **输出参数: NONE
 **返    回: 长度
 **实现描述: 
 **注意事项: 
 **修    改: # Qifeng.zou # 2014.09.11 #
 ******************************************************************************/
static int log2_print_dump(log2_cycle_t *log, const void *dump, int dumplen)
{
    const char *dump_ptr, *dump_end;
    unsigned char var[2] = {0, 31};    
    int idx, n = 0, row, count = 0, rows;

    dump_ptr = (const char *)dump;
    dump_end = dump + dumplen;                  /* 内存结束地址 */
    rows = (dumplen - 1)/LOG_DUMP_COL_NUM;      /* 每页行数 */

    while(dump_ptr < dump_end) 
    {        
        for(row=0; row<=rows; row++) 
        {
            /* 1. 判断是否打印头部字串 */
            if(0 == (row + 1)%LOG_DUMP_PAGE_MAX_ROWS)
            {
                fprintf(log->fp, "%s", LOG_DUMP_HEAD_STR);
            }

            /* 2. 计算偏移量 */
            count = row * LOG_DUMP_COL_NUM;            

            /* 3. 将信息写入缓存 */
            fprintf(log->fp, "%05d", count);
            fprintf(log->fp, "(%05x) ", count);        

            /* >>3.1 16进制打印一行 */
            for(idx=0; (idx<LOG_DUMP_COL_NUM) && (dump_ptr<dump_end); idx++)
            {
                fprintf(log->fp, "%02x ", *dump_ptr);
                dump_ptr++;
            }        

            /* >>3.2 最后数据不足一行时，使用空格补上 */
            for(n=0; n<LOG_DUMP_COL_NUM-idx; n++) 
            {
                fprintf(log->fp, "   ");
            }            

            fprintf(log->fp, " ");            
            dump_ptr -= idx;

            /* >>3.3 以字符方式打印信息 */
            for(n=0; n<idx; n++) 
            {
                if(((unsigned char)(*dump_ptr) <= (var[1])) 
                    && ((unsigned char)(*dump_ptr) >= (var[0]))) 
                {
                    fprintf(log->fp, "*");            
                }     
                else 
                {                    
                    fprintf(log->fp, "%c", *dump_ptr);            
                }                
                dump_ptr++;
            }        

            fprintf(log->fp, "\n");            
        } /* dump_end of for    */    
    } /* dump_end of while    */    

    return 0;
}
