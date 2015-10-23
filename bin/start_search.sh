#!/bin/sh

ulimit -c unlimited
source ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 准备阶段
sudo redis-server /etc/redis/redis_slave_6380.conf

# 启动服务
./invertd -d    # 倒排服务
sleep 5
./frwder -N SendToInvertd -d     # 转发服务
sleep 1
./listend -N SearchEngineListend -d    # 代理服务

./watch.sh      # 监控进程状态
