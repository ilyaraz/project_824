#pragma once

#include "ServerConnection.h"
#include "ConsistentHashing.h"

#include <KVStorage.h>
#include <ViewService.h>

#include <boost/optional.hpp>

#include <stdexcept>
#include <string>
#include <iostream>

class CacheClient {
public:
    CacheClient(const std::string &view_server, const int &view_port) {
        try {
            ServerConnection<ViewServiceClient> connection(view_server, view_port);
            GetServersReply reply;
            connection.getClient()->getView(reply);
            servers = reply.servers;
            std::cout << servers.size() << " servers in charge" << std::endl;
        }
        catch (...) {
            throw std::runtime_error("filed to query view server");
        }
    }

    boost::optional<std::string> get(const std::string &key) {
        try {
            int serverID = getServerID(key);
            ServerConnection<KVStorageClient> connection(servers[serverID].server, servers[serverID].port);
            GetArgs args;
            args.key = key;
            GetReply reply;
            connection.getClient()->get(reply, args);

            if (reply.status == Status::OK) {
                return boost::optional<std::string>(reply.value);
            }
            return boost::optional<std::string>();
        }
        catch (...) {
            throw std::runtime_error("get() failed");
        }
    }

    void put(const std::string &key, const std::string &value) {
        try {
            int serverID = getServerID(key);
            ServerConnection<KVStorageClient> connection(servers[serverID].server, servers[serverID].port);
            PutArgs args;
            args.key = key;
            args.value = value;
            PutReply reply;
            connection.getClient()->put(reply, args);
        }
        catch (...) {
            throw std::runtime_error("put() failed");
        }
    }
private:
    std::vector<Server> servers;

    int getServerID(const std::string &key) {
        unsigned int result = 0;
        for (size_t i = 0; i < key.size(); ++i) {
            result ^= (unsigned int) key[i];
            result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
        }
        return result % servers.size();
    }
};
