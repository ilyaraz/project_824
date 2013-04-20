#!/bin/sh

./with_thrift.sh ./server 10001 &
./with_thrift.sh ./server 10002 &
./with_thrift.sh ./server 10003 &
./with_thrift.sh ./server 10004 &
./with_thrift.sh ./server 10005 &

./with_thrift.sh ./view 4057 servers.txt &
