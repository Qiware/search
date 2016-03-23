#!/bin/sh

ulimit -c unlimited
. ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 启动服务
./invertd -l error -d   # 倒排服务
sleep 5
#./frwder -l error -c ../conf/frwder.xml -d     # 转发服务
sleep 1
./listend -l error -c ../conf/listend.xml -d   # 代理服务

./watch.sh      # 监控进程状态