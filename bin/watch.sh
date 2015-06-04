#!/bin/sh

###############################################################################
# 功能描述: 打印进程状态
# 参数说明: NONE
# 输出结果: 进程统计信息
# 注意事项:
# 作    者: # Qifeng.zou # 2014.11.28 #
###############################################################################
print_proc()
{
    ps -axu | grep -e "crawler" \
				   -e "filter"  \
				   -e "logsvr" \
				   -e "redis" \
				   -e "agentd" \
				   -e "invertd" \
				   -e "frwd_exec" \
				   -e "sdtp" | grep -v "grep" | sort
}

###############################################################################
# 功能描述: 打印共享内存状态
# 参数说明: NONE
# 输出结果: 共享内存状态信息
# 注意事项:
# 作    者: # Qifeng.zou # 2014.11.28 #
###############################################################################
print_shm()
{
    ipcs -m | awk '{ if ($6 ~ /nattch/ || $6 >= 1) { print $0 }}' | grep -v "grep"
}

###############################################################################
# 功能描述: 打印指定进程的网络状态
# 参数说明:
#   参数1: 进程名
# 输出结果: 网络状态统计信息
# 注意事项:
#   1. 暂时不精确匹配
#       1) 精确匹配: grep -e "$item " -e "$item"$
#       2) 非精确匹配: grep "$item"
#   2. 文件netstat.list作为当前网络状态输入文件
# 作    者: # Qifeng.zou # 2014.11.28 #
###############################################################################
print_netstat()
{
    rm -f .netstat.ls
    sudo netstat -antp | grep -v grep  > .netstat.all

    for item in $@ # 遍历参数列表
    do
        total=0
        listen_num=0
        active_num=0
        close_num=0

        while read line # 遍历文件行
        do
            num=`echo $line | grep "/$item" | wc -l`
            if [ $num -eq 0 ]; then
                continue;
            fi

            # 统计套接字总数
            total=`expr $total + $num`

            # 统计活跃的套接字
            num=`echo $line | grep 'LISTEN' | wc -l`
            if [ $num -gt 0 ]; then
                listen_num=`expr $listen_num + $num`
            fi

            # 统计活跃的套接字
            num=`echo $line | grep 'ESTABLISHED' | wc -l`
            if [ $num -gt 0 ]; then
                active_num=`expr $active_num + $num`
            fi

            # 统计关闭的套接字
            num=`echo $line | grep -e 'CLOSE_WAIT' -e 'SYN_SENT' -e 'FIN_WAIT1' -e 'FIN_WAIT2' -e 'LAST_ACK' | wc -l`
            if [ $num -gt 0 ]; then
                close_num=`expr $close_num + $num`
            fi
        done < .netstat.all

        # 打印统计信息
        len=`expr length "$item"`
        if [ $len -lt 8 ]; then
            echo "$item\t\t$listen_num\t$active_num\t$close_num\t$total" >> .netstat.ls
        elif [ $len -lt 16 ]; then
            echo "$item\t$listen_num\t$active_num\t$close_num\t$total" >> .netstat.ls
        else
            echo "$item\t$listen_num\t$active_num\t$close_num\t$total" >> .netstat.ls
        fi
    done
}

###############################################################################
# 功能描述: 系统监控主程序
# 参数说明: NONE
# 输出结果: 系统监控信息
# 注意事项:
# 作    者: # Qifeng.zou # 2014.11.28 #
###############################################################################
main()
{
    #touch .netstat.ls

    while true
    do
        clear
        echo "Copyright(C) 2014-2024 XunDao Technology Co.,Ltd"

        echo ""
        echo "Process stat:"
        echo "---------------------------------------------------------------------"
        echo "USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND"
        print_proc
        echo "---------------------------------------------------------------------"

        #echo ""
        #echo "Shared memory stat:"
        #echo "---------------------------------------------------------------------"
        #print_shm
        #echo "---------------------------------------------------------------------"

        echo ""
        echo "Disk stat:"
        echo "---------------------------------------------------------------------"
        sudo df -l -BG
        echo "---------------------------------------------------------------------"

        # 显示上次获取的netstat信息(防止刷屏时间过长)
        #echo ""
        #echo "Network stat:"
        #echo "---------------------------------------------------------------------"
        #echo "PROC\t\tLISTEN\tESTAB\tCLOSE\tTOTAL"

        #cat .netstat.ls
        #echo "---------------------------------------------------------------------"
        #print_netstat "crawler" "filter" "redis-server" "search"

        sleep 2
    done;
}

main
