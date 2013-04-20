GENERATED_SOURCES = ./gen-cpp/KVServer.cpp ./gen-cpp/kvserver_types.cpp ./gen-cpp/kvserver_constants.cpp
COMPILER_FLAGS = -O3 -Wall
INCLUDE_DIRS = -I./gen-cpp -I./thrift-0.9.0/lib/cpp/src
INCLUDE_INJECTIONS = -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H
LIB_DIRS = -L./thrift-0.9.0/lib/cpp/.libs -lthrift
CPP_COMPILER = g++

.PHONY: all clean

all: server client

server: server.cpp gen-cpp
	${CPP_COMPILER} server.cpp ${GENERATED_SOURCES} -o server ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

client: client.cpp gen-cpp
	${CPP_COMPILER} client.cpp ${GENERATED_SOURCES} -o client ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

gen-cpp: kvserver.thrift
	thrift-0.9.0/compiler/cpp/thrift -gen cpp kvserver.thrift

clean:
	rm -rf gen-cpp server client