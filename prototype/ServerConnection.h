#pragma once

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

//using namespace apache::thrift;
//using namespace apache::thrift::protocol;
//using namespace apache::thrift::transport;

using boost::shared_ptr;

template<typename T> class ServerConnection {
public:
    ServerConnection(const std::string &server, const int &port) {
        socket = shared_ptr<apache::thrift::transport::TSocket>(new apache::thrift::transport::TSocket(server, port));
        transport = shared_ptr<apache::thrift::transport::TTransport>(new apache::thrift::transport::TFramedTransport(socket));
        protocol = shared_ptr<apache::thrift::protocol::TProtocol>(new apache::thrift::protocol::TBinaryProtocol(transport));

        client = shared_ptr<T>(new T(protocol));
        transport->open();
    }

    ~ServerConnection() {
        transport->close();
    }

    shared_ptr<T> getClient() const {
        return client; 
    }
private:
    shared_ptr<apache::thrift::transport::TSocket> socket;
    shared_ptr<apache::thrift::transport::TTransport> transport;
    shared_ptr<apache::thrift::protocol::TProtocol> protocol;
    shared_ptr<T> client;
};
