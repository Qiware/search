#!/bin/sh

pkill -9 crawler
pkill -9 crawler-filter
pkill -9 search

sleep 1

ipcs -m | grep -v 'dest' | awk '{ print $0 }'

redis-cli FLUSHALL
