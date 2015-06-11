#!/bin/sh

pkill -9 mmexec
pkill -9 crawler
pkill -9 filter
pkill -9 listend
pkill -9 monitor
pkill -9 invertd
pkill -9 logsvr
pkill -9 frwder
pkill -9 rttp_recv
pkill -9 rttp_send
pkill -9 sdtp_recv
pkill -9 sdtp_send

sleep 1

# 删除共享内存
list=`ipcs -m | grep -v 'dest' | awk '{ if ($6 == 0) { print $2 }}'`
for shm in $list
do
    echo $shm
    ipcrm -m $shm
done

# 显示共享内存
ipcs -m | grep -v 'dest' | awk '{ print $0 }'

redis-cli FLUSHALL
