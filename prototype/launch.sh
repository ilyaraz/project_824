#!/bin/sh

./with_thrift.sh ./view 4057 &
sleep 0.5
./with_thrift.sh ./server 127.0.0.1 10001 127.0.0.1 4057 &
sleep 0.5
./with_thrift.sh ./server 127.0.0.1 10002 127.0.0.1 4057 &
sleep 0.5
./with_thrift.sh ./server 127.0.0.1 10003 127.0.0.1 4057 &
sleep 0.5
./with_thrift.sh ./server 127.0.0.1 10004 127.0.0.1 4057 &
sleep 0.5
./with_thrift.sh ./server 127.0.0.1 10005 127.0.0.1 4057 &

