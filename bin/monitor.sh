#!/bin/sh


while true
do
    clear
    echo "Copyright(C) 2014-2024 XunDao Technology Co.,Ltd"

    echo ""
    echo "Process list:"
    echo "---------------------------------------------------------------------"
    ps -axu | grep -e "crawler" -e "logsvr" -e "redis" | grep -v grep
    echo "---------------------------------------------------------------------"

    echo ""
    echo "Shared memory list:"
    echo "---------------------------------------------------------------------"
    ipcs -m | grep -v "dest" | grep "qifeng"
    echo "---------------------------------------------------------------------"

    echo ""
    echo "Net stat list:"
    echo "---------------------------------------------------------------------"
    sudo netstat -antp | grep -e "crawler" | grep -v grep | wc -l
    sudo netstat -antp | grep -e "crawler" | grep -v grep | grep -v "WAIT" | wc -l
    echo "---------------------------------------------------------------------"

    sleep 1
done;
