#include <KVStorage.h>
#include <ViewService.h>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>

#include <iostream>
#include <string>
#include <cstdlib>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using boost::shared_ptr;

using namespace boost::posix_time;

class CacheClient {
public:
    CacheClient(const std::string &view_server, const int &view_port) {
		shared_ptr<TSocket> socket(new TSocket(view_server, view_port));
		shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		ViewServiceClient client(protocol);
		transport->open();
        GetServersReply reply;
        client.getView(reply);
        transport->close();

        servers = reply.servers;

        std::cout << servers.size() << " servers in charge" << std::endl;
    }

    boost::optional<std::string> get(const std::string &key) {
        int serverID = getServerID(key);
		shared_ptr<TSocket> socket(new TSocket(servers[serverID].server, servers[serverID].port));
		shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		KVStorageClient client(protocol);
        transport->open();
        GetArgs args;
        args.key = key;
        GetReply reply;
        client.get(reply, args);
        transport->close();

        if (reply.status == Status::OK) {
            return boost::optional<std::string>(reply.value);
        }
        return boost::optional<std::string>();
    }

    void put(const std::string &key, const std::string &value) {
        int serverID = getServerID(key);
		shared_ptr<TSocket> socket(new TSocket(servers[serverID].server, servers[serverID].port));
		shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		KVStorageClient client(protocol);
        transport->open();
        PutArgs args;
        args.key = key;
        args.value = value;
        PutReply reply;
        client.put(reply, args);
        transport->close();
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

int main(int argc, char **argv) {

    srand(0xdead);

	std::string server = argv[1];
	int port = atoi(argv[2]);

    CacheClient cacheClient(server, port);

    int cnt = 0;
    ptime t1(microsec_clock::local_time());
    for (;;) {
        std::string key;
        for (int i = 0; i < 6; ++i) {
            key += '0' + (rand() % 10);
        }
        if (rand() % 2) {
            std::string value;
            for (int i = 0; i < 1024; i++) {
                value += '0' + (rand() % 10);
            }
            cacheClient.put(key, value);
        }
        else {
            if (cacheClient.get(key)) {
            }
        }
        ++cnt;
        if (cnt % 1000 == 0) {
            ptime t2(microsec_clock::local_time());
            double x = (t2 - t1).total_milliseconds();
            std::cout << cnt << " " << x / cnt << std::endl; 
        }
    }
	return 0;
}
