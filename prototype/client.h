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
    CacheClient(const Server &viewServer): viewServer(viewServer) {
        getView();
    }

    boost::optional<std::string> get(const std::string &key) {
        try {
            Server server = getServerID(key);
            ServerConnection<KVStorageClient> connection(server.server, server.port);
            GetArgs args;
            args.key = key;
            args.viewNum = reply_.viewNum;
            GetReply reply;
            connection.getClient()->get(reply, args);

            if (reply.status == Status::OK) {
                return boost::optional<std::string>(reply.value);
            }
            else if (reply.status == Status::WRONG_SERVER) {
                getView();
                throw std::runtime_error("get(): wrong server");
            }
            return boost::optional<std::string>();
        }
        catch (...) {
            throw std::runtime_error("get() failed");
        }
    }

    void put(const std::string &key, const std::string &value) {
        try {
            Server server = getServerID(key);
            ServerConnection<KVStorageClient> connection(server.server, server.port);
            PutArgs args;
            args.key = key;
            args.value = value;
            args.viewNum = reply_.viewNum;
            PutReply reply;
            connection.getClient()->put(reply, args);

            if (reply.status == Status::WRONG_SERVER) {
                getView();
                throw std::runtime_error("put(): wrong server");
            }
        }
        catch (...) {
            throw std::runtime_error("put() failed");
        }
    }
private:
    GetServersReply reply_;
    Server viewServer;

    Server getServerID(const std::string &key) {
        return getServer(hash(key), reply_.view);
    }

    void getView() {
        try {
            ServerConnection<ViewServiceClient> connection(viewServer.server, viewServer.port);
            connection.getClient()->getView(reply_);
            std::cout << "view " << reply_.viewNum << " " << reply_.view.servers.size() << " servers in charge" << std::endl;
        }
        catch (...) {
            throw std::runtime_error("failed to query view server");
        }
    }
};
