#!/bin/sh

ulimit -c unlimited
. ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 准备阶段
#sudo redis-server /etc/redis/redis_slave_6380.conf

# 启动服务

./logsvr -L ../temp/log.key  -d     # 日志服务
#./mmexec -l trace -L ../temp/log.key  -d     # 内存服务

./watch.sh      # 监控进程状态
