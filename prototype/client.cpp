#include <KVServer.h>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include <iostream>
#include <string>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

int main(int argc, char **argv) {

	for (;;) {

		std::string cmd;
		std::cin >> cmd;

		if (cmd != "put" && cmd != "get") {
			std::cout << "put or get" << std::endl;
			continue;
		}

		std::string key, value;
		std::cin >> key;
		if (cmd == "put") {
			std::cin >> value;
		}

		boost::shared_ptr<TSocket> socket(new TSocket(argv[1], atoi(argv[2])));
		boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		KVServerClient client(protocol);
		transport->open();

		if (cmd == "get") {
			GetArgs args;
			args.key = key;
			GetReply reply;
			client.get(reply, args);
			if (reply.status == Status::OK) {
				std::cout << reply.value << std::endl;
			}
			else {
				std::cout << "no such key" << std::endl;
			}
		}
		else {
			PutArgs args;
			args.key = key;
			args.value = value;
			PutReply reply;
			client.put(reply, args);
			std::cout << "entry has been updated" << std::endl;
		}
		transport->close();
	}

	return 0;
}