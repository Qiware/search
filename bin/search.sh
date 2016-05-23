#!/bin/sh

./kill.sh

ulimit -c unlimited
. ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 启动服务
# 注意: 绝不允许出现相同结点ID的服务, 否则可能出现链路不通的问题.
./frwder -l error -c ../conf/frwder.xml -d     # 转发服务
./invertd -l error -c ../conf/invertd.xml -d   # 倒排服务
./invertd -l error -c ../conf/invertd-30002.xml -d   # 倒排服务

./listend -l error -c ../conf/listend.xml -d   # 代理服务

./watch.sh      # 监控进程状态
