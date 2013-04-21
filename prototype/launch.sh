#!/bin/sh

./with_thrift.sh ./server 10001 &

./with_thrift.sh ./view 4057 servers.txt &
