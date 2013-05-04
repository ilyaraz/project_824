#include "client.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <stdexcept>

#include <unistd.h>

const int KEY_LENGTH = 7;
const int PUT_PROPORTION = 50;
const int VALUE_SIZE = 1024;

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
        for (int i = 0; i < KEY_LENGTH; ++i) {
            key += '0' + (rand() % 10);
        }
        if (rand() % PUT_PROPORTION == 0) {
            std::string value;
            for (int i = 0; i < VALUE_SIZE; i++) {
                value += '0' + (rand() % 10);
            }
            try {
                cacheClient.put(key, value);
            }
            catch (std::runtime_error &e) {
                std::cout << e.what() << std::endl;
                ++numFailedPuts;
                std::cout << "put failure on key " << key << std::endl;
            }
        }
        else {
            try {
                if (cacheClient.get(key)) {
                }
            }
            catch (std::runtime_error &e) {
                std::cout << e.what() << std::endl;
                ++numFailedGets;
                std::cout << "get failure on key " << key << std::endl;
            }
        }
        ++cnt;
    }
	return 0;
}
