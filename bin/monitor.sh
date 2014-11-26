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
    total=`sudo netstat -antp | grep "crawler" | grep -v "crawler-filter"| grep -v grep | wc -l`
    established=`sudo netstat -antp | grep "crawler" | grep -v "crawler-filter" | grep "ESTABLISHED" | wc -l`
    close_wait=`sudo netstat -antp | grep "crawler" | grep -v "crawler-filter" | grep "CLOSE_WAIT" | wc -l`
    echo "PROC\t\tESTABLISHED\tCLOSE_WAIT\tTOTAL"
    echo "crawler\t\t$established\t\t$close_wait\t\t$total"
    echo "---------------------------------------------------------------------"

    sleep 1
done;
