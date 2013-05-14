#!/bin/sh
for i in {1..20}; do ./with_thrift.sh ./stress 128.30.48.203 4057 & done
