#include <KVStorage.h>

#include <thrift/protocol/TBinaryProtocol.h>
//#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using boost::shared_ptr;

class KVStorageHandler: virtual public KVStorageIf {
public:
	KVStorageHandler() {}

	void put(PutReply& _return, const PutArgs& query) {
		data[query.key] = query.value;
		_return.status = Status::OK;

        if (data.size() % 10000 == 0) {
            std::cout << data.size() << " entries" << std::endl;
        }
	}

	void get(GetReply& _return, const GetArgs& query) {
		if (data.count(query.key)) {
			_return.status = Status::OK;
			_return.value = data[query.key];
		}
		else {
			_return.status = Status::NO_KEY;
		}
	}

private:
	std::map<std::string, std::string> data;
};

int main(int argc, char **argv) {
	int port = atoi(argv[1]);

	shared_ptr<KVStorageHandler> handler(new KVStorageHandler());
	shared_ptr<TProcessor> processor(new KVStorageProcessor(handler));
	//shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	//shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	//TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    TNonblockingServer server(processor, protocolFactory, port);
	server.serve();
	return 0;
}
