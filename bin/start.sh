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

# 启动服务
./mmexec        # 内存服务
./invtd -d      # 倒排服务
./frwder -d     # 转发服务
./agentd -d     # 代理服务
#./crawler -d    # 爬虫服务
#./filter -d     # 爬虫过滤服务
./watch.sh      # 监控进程状态
