#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../lib

ulimit -c unlimited

# 日志服务
./logsvr

# 爬虫服务
./crawler -d

# 爬虫过滤服务
./crawler-filter -d
