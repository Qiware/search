#!/bin/sh

sudo su # 获取ROOT权限

ulimit -c unlimited

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 准备阶段
sudo redis-server /etc/redis/redis_slave_6380.conf

# 日志服务
n=`ps -axu | grep "logsvr" | wc -l`
if [ $n -gt 0 ]; then
    ./logsvr
fi

# 爬虫服务
./crawler -d

# 爬虫过滤服务
./filter -d

# 搜索引擎服务
./search -d

# 监控进程状态
./watch.sh
