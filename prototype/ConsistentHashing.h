#include <stdio.h>
#include <string.h>
#include "cache_types.h"
#include <cassert>
#include <sstream>

int hash(const std::string &key) {
    unsigned int result = 0;
    size_t s = key.size();
    for (size_t i = 0; i < s; ++i) {
        result ^= (unsigned int) key[i];
        result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
    }
    return result;
}

int hash(const Server &server) {
    std::ostringstream oss;
    oss << server.server << ":" << server.port << "random enough salt";
    return hash(oss.str());
}

Server getServer(int hash, const View& v) {
    assert(!v.hashToServer.empty());
    std::map<int, Server>::const_iterator it = v.hashToServer.lower_bound(hash);
    if (it == v.hashToServer.end()) {
        return v.hashToServer.begin()->second;
    }
    return it->second;
}
