#include "client.h"

#include <string>
#include <iostream>

int main(int argc, char **argv) {
    std::string server = argv[1];
    int port = atoi(argv[2]);
    CacheClient client(server, port);
    for (;;) {
        std::string cmd, key;
        std::cin >> cmd >> key;
        if (cmd == "get") {
            boost::optional<std::string> value = client.get(key);
            if (!value) {
                std::cout << "no such key" << std::endl;
            }
            else {
                std::cout << *value << std::endl;
            }
        }
        else if (cmd == "put") {
            std::string value;
            std::cin >> value;
            client.put(key, value);
        }
    }
    return 0;
}
