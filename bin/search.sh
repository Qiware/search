#!/bin/sh

./kill.sh

ulimit -c unlimited
. ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 启动服务
./frwder -l debug -c ../conf/frwder.xml -d     # 转发服务
./invertd -l debug -c ../conf/invertd.xml -d   # 倒排服务
./invertd -l debug -c ../conf/invertd-30002.xml -d   # 倒排服务
./listend -l debug -c ../conf/listend.xml -d   # 代理服务

./watch.sh      # 监控进程状态
