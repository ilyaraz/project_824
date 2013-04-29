GENERATED_SOURCES = ./gen-cpp/ViewService.cpp ./gen-cpp/KVStorage.cpp ./gen-cpp/cache_types.cpp ./gen-cpp/cache_constants.cpp
COMPILER_FLAGS = -O3 -Wall
INCLUDE_DIRS = -I./gen-cpp -I./thrift-0.9.0/lib/cpp/src
INCLUDE_INJECTIONS = -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H
LIB_DIRS = -L./thrift-0.9.0/lib/cpp/.libs -lthrift -lthriftnb -levent
CPP_COMPILER = g++

.PHONY: all clean

all: server stress view monitor console gen-py gen-java

server: server.cpp gen-cpp
	${CPP_COMPILER} server.cpp ${GENERATED_SOURCES} -o server ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

stress: stress.cpp gen-cpp ServerConnection.h client.h
	${CPP_COMPILER} stress.cpp ${GENERATED_SOURCES} -o stress ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

console: console.cpp gen-cpp ServerConnection.h client.h
	${CPP_COMPILER} console.cpp ${GENERATED_SOURCES} -o console ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

view: view.cpp gen-cpp ServerConnection.h
	${CPP_COMPILER} view.cpp ${GENERATED_SOURCES} -o view ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

monitor: monitor.cpp gen-cpp ServerConnection.h
	${CPP_COMPILER} monitor.cpp ${GENERATED_SOURCES} -o monitor ${COMPILER_FLAGS} ${INCLUDE_DIRS} ${INCLUDE_INJECTIONS} ${LIB_DIRS}

gen-cpp: cache.thrift
	thrift-0.9.0/compiler/cpp/thrift -gen cpp cache.thrift

gen-py: cache.thrift
	thrift-0.9.0/compiler/cpp/thrift -gen py cache.thrift

gen-java: cache.thrift
	thrift-0.9.0/compiler/cpp/thrift -gen java cache.thrift

clean:
	rm -rf gen-cpp server stress view monitor console gen-py gen-java
