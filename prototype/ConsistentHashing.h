#include <stdio.h>
#include <string.h>
#include "cache_types.h"
#include <cassert>
#include <sstream>
#include <stdexcept>

class EmptyRingException: public std::runtime_error {
public:
    explicit EmptyRingException(const std::string &what): std::runtime_error(what) {}
};

int hash(const std::string &key) {
    unsigned int result = 0;
    size_t s = key.size();
    for (size_t i = 0; i < s; ++i) {
        result ^= (unsigned int) key[i];
        result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
    }
    return result;
}

std::vector<Server> getServer(int hash, const View& v) {
    if (v.hashToServer.empty()) {
        throw EmptyRingException("empty ring");
    }
    std::map<int, std::vector<Server> >::const_iterator it = v.hashToServer.lower_bound(hash);
    if (it == v.hashToServer.end()) {
        return v.hashToServer.begin()->second;
    }
    return it->second;
}
