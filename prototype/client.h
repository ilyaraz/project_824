#pragma once

#include "ServerConnection.h"
#include "ConsistentHashing.h"

#include <KVStorage.h>
#include <ViewService.h>

#include <boost/optional.hpp>

#include <stdexcept>
#include <string>
#include <iostream>

class ViewServiceException: public std::runtime_error {
public:
    explicit ViewServiceException(const std::string &what): std::runtime_error(what) {}
};

class WrongServerException: public std::runtime_error {
public:
    explicit WrongServerException(const std::string &what): std::runtime_error(what) {}
};

class ConnectionFailedException: public std::runtime_error {
public:
    explicit ConnectionFailedException(const std::string &what): std::runtime_error(what) {}
};

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
                throw WrongServerException("wrong server"); 
            }
            return boost::optional<std::string>();
        }
        catch (EmptyRingException &e) {
            getView();
            throw;
        }
        catch (ViewServiceException &e) {
            throw;
        }
        catch (WrongServerException &e) {
            throw;
        }
        catch (...) {
            getView();
            throw ConnectionFailedException("failed connection"); 
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
                throw WrongServerException("wrong server"); 
            }
        }
        catch (EmptyRingException &e) {
            getView();
            throw;
        }
        catch (ViewServiceException &e) {
            throw;
        }
        catch (WrongServerException &e) {
            throw;
        }
        catch (...) {
            getView();
            throw ConnectionFailedException("failed connection"); 
        }
    }
private:
    GetServersReply reply_;
    Server viewServer;

    Server getServerID(const std::string &key) {
        return getServer(hash(key), reply_.view)[0];
    }

    void getView() {
        try {
            ServerConnection<ViewServiceClient> connection(viewServer.server, viewServer.port);
            connection.getClient()->getView(reply_);
            std::cout << "view " << reply_.viewNum << " " << reply_.view.hashToServer.size() << " servers in charge" << std::endl;
        }
        catch (...) {
            throw ViewServiceException("failed to query view service");
        }
    }
};
