#include "client.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <stdexcept>

#include <unistd.h>

int main(int argc, char **argv) {

    srand(getpid());

	std::string server = argv[1];
	int port = atoi(argv[2]);

    Server viewServer;
    viewServer.server = server;
    viewServer.port = port;

    CacheClient cacheClient(viewServer);

    int cnt = 0;
    int numFailedPuts = 0;
    int numFailedGets = 0;
    for (;;) {
        std::string key;
        for (int i = 0; i < 7; ++i) {
            key += '0' + (rand() % 10);
        }
        if (rand() % 2) {
            std::string value;
            for (int i = 0; i < 10240; i++) {
                value += '0' + (rand() % 10);
            }
            try {
                cacheClient.put(key, value);
            }
            catch (std::runtime_error &e) {
                ++numFailedPuts;
            }
        }
        else {
            try {
                if (cacheClient.get(key)) {
                }
            }
            catch (std::runtime_error &e) {
                ++numFailedGets;
            }
        }
        ++cnt;
    }
	return 0;
}
