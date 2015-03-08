#!/bin/sh

# 删除共享内存
list=`ipcs -m | grep -v 'dest' | awk '{ if ($6 == 0) { print $2 }}'`
for shm in $list
do
    ipcrm -m $shm
done

redis-cli FLUSHALL
rm -fr ../data  ../log/* smtc*.log*

# 显示共享内存
ipcs -m | grep -v 'dest' | awk '{ print $0 }'
