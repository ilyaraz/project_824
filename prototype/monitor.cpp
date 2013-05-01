#include "ServerConnection.h"

#include <ViewService.h>

#include <unistd.h>
#include <iostream>

long long getOps(const GetStatisticsReply &s) {
    return s.numInsertions + s.numUpdates + s.numGoodGets + s.numFailedGets;
}

void printStatistics(const GetStatisticsReply &statistics, const GetStatisticsReply &last) {
    std::cout << "ops: " << getOps(statistics) - getOps(last) << std::endl;
    std::cout << "data size: " << statistics.dataSize << std::endl;
    std::cout << "hash tables sizes: " << statistics.memoryOverhead << std::endl;
    std::cout << "evictions: " << statistics.numEvictions << std::endl;
    std::cout << "purges: " << statistics.numPurges << std::endl;
    std::cout << std::endl;
}

int main(int argc, char **argv) {
    std::string server = argv[1];
    int port = atoi(argv[2]);
    GetStatisticsReply last;
    for (;;) {
        ServerConnection<ViewServiceClient> connection(server, port);
        GetStatisticsReply statistics;
        connection.getClient()->getStatistics(statistics);
        printStatistics(statistics, last);
        last = statistics;
        sleep(1);
    }
    return 0;
}
