#include <stdio.h>
#include <string.h>
#include "cache_types.h"
#include <cassert>

int hash(const std::string &key) {
    unsigned int result = 0;
    size_t s = key.size();
    for (size_t i = 0; i < s; ++i) {
        result ^= (unsigned int) key[i];
        result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
    }
    return result;
}

Server getServer(int hash, const View& v) {
    assert(!v.hashToServer.empty());
    std::map<int, Server>::iterator it = v.hashToServer.lower_bound(hash);
    if (it == v.hashToServer.end()) {
        return *v.hashToServer.begin();
    }
    return *it;
}
