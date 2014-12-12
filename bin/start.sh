#!/bin/sh
ulimit -c unlimited

# 准备阶段
sudo redis-server /etc/redis/redis_slave_6380.conf

# 日志服务
./logsvr

# 爬虫服务
./crawler -d

# 爬虫过滤服务
./crawler-filter -d

# 搜索引擎服务
./search -d

./monitor.sh
