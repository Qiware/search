#!/bin/sh


while true
do
    clear
    echo "Copyright(C) 2014-2024 XunDao Technology Co.,Ltd"

    echo ""
    echo "Process stat:"
    echo "---------------------------------------------------------------------"
    echo "USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND"
    ps -axu | grep -e "crawler" -e "logsvr" -e "redis" | grep -v grep | sort
    echo "---------------------------------------------------------------------"

    echo ""
    echo "Shared memory stat:"
    echo "---------------------------------------------------------------------"
    ipcs -m | grep -v "dest" | grep "qifeng"
    echo "---------------------------------------------------------------------"

    echo ""
    echo "Net stat:"
    echo "---------------------------------------------------------------------"
    IFS='\n'
    total=0
    active_num=0
    close_num=0

    sudo netstat -antp | grep 'crawler' | grep -v 'crawler-filter'| grep -v grep  > proc.list

    for item in `cat ./proc.list`;
    do
        flag=`echo $item  | wc -l`
        if [ $flag -eq 0 ]; then
            continue;
        fi

        # 统计套接字总数
        total=`expr $total + $flag`

        # 统计活跃的套接字
        flag=`echo $item | grep 'ESTABLISHED' | wc -l`
        if [ $flag -gt 0 ]; then
            active_num=`expr $active_num + $flag`
        fi

        # 统计关闭的套接字
        flag=`echo $item | grep -e 'CLOSE_WAIT' -e 'SYN_SENT' | wc -l`
        if [ $flag -gt 0 ]; then
            close_num=`expr $close_num + $flag`
        fi
    done

    echo "PROC\t\tESTABLISHED\tCLOSE\t\tTOTAL"
    echo "crawler\t\t$active_num\t\t$close_num\t\t$total"
    echo "---------------------------------------------------------------------"

    sleep 1
done;
