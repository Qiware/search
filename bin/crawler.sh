#!/bin/sh

ulimit -c unlimited
. ./prepare.sh

export LD_LIBRARY_PATH='/usr/lib/x86_64-linux-gnu:../lib'

# 准备阶段
sudo redis-server /etc/redis/redis_slave_6380.conf

# 启动服务

./crawler -l trace -c ../conf/crawler.xml -d   # 爬虫服务
./filter -l trace -c ../conf/filter.xml -d     # 爬虫过滤服务

./watch.sh     # 监控进程状态
