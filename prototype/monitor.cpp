#include "ServerConnection.h"

#include <ViewService.h>

#include <unistd.h>
#include <iostream>

void printStatistics(const GetStatisticsReply &statistics) {
    std::cout << "insertions: " << statistics.numInsertions << std::endl;
}

int main(int argc, char **argv) {
    std::string server = argv[1];
    int port = atoi(argv[2]);
    for (;;) {
        ServerConnection<ViewServiceClient> connection(server, port);
        GetStatisticsReply statistics;
        connection.getClient()->getStatistics(statistics);
        printStatistics(statistics);
        sleep(1);
    }
    return 0;
}
