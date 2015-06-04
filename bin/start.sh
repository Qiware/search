#!/bin/sh

ulimit -c unlimited

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 准备阶段
sudo redis-server /etc/redis/redis_slave_6380.conf

# 日志服务
n=`ps -axu | grep "logsvr" | wc -l`
if [ $n -gt 0 ]; then
    ./logsvr
fi

# 发送服务
./sdtp_send 28888 &

# 爬虫服务
#./crawler -d

# 爬虫过滤服务
#./filter -d

# 探针服务
./probd -d

# 监控进程状态
./watch.sh
