#include "lock.h"

/******************************************************************************
 **函数名称: _try_flock
 **功    能: 尝试加锁(文件锁)
 **输入参数:
 **     fd: 文件描述符
 **     type: 锁类型(读:F_RDLCK, 写:F_WRLCK, 解锁:F_UNLCK)
 **     whence: 相对位置(SEEK_SET, CURR_SET, END_SET)
 **     start: 其实位置
 **     len: 加锁长度
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述:
 **注意事项:
 **     F_SETLK 设置文件锁定的状态。
 **         此时flcok 结构的l_type 值必须是F_RDLCK、F_WRLCK或F_UNLCK。
 **         如果无法建立锁定，则返回-1，错误代码为EACCES 或EAGAIN。
 **作    者: # Qifeng.zou # 2013.09.06 #
 ******************************************************************************/
int _try_flock(int fd, int type, int whence, int start, int len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = start;
    fl.l_len = len;

    return fcntl(fd, F_SETLK, &fl);
}

/******************************************************************************
 **函数名称: _flock
 **功    能: 阻塞加读写锁(文件锁)
 **输入参数:
 **     fd: 文件描述符
 **     type: 锁类型(读锁:F_RDLCK, 写锁:F_WRLCK, 解锁:F_UNLCK)
 **     whence: 相对位置(SEEK_SET, CURR_SET, END_SET)
 **     offset: 偏移位置
 **     len: 加锁长度
 **输出参数: NONE
 **返    回: 0:success !0:failed
 **实现描述:
 **注意事项:
 **     1) 如果offset和len都为0时, 表示锁住整个文件.
 **     2) F_SETLKW 与F_SETLK作用相似，
 **         但是无法建立锁定时，此调用会一直等到锁定动作成功为止。
 **         若在等待锁定的过程中被信号中断时，会立即返回-1，错误代码为EINTR。
 **作    者: # Qifeng.zou # 2013.09.06 #
 ******************************************************************************/
int _flock(int fd, int type, int whence, int offset, int len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = offset;
    fl.l_len = len;

    return fcntl(fd, F_SETLKW, &fl);
}
