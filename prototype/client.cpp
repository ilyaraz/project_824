#include <KVServer.h>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <cstdlib>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using boost::shared_ptr;

using namespace boost::posix_time;

int main(int argc, char **argv) {

	srand(0xdead);

	std::string server = argv[1];
	int port = atoi(argv[2]);

	int cnt = 0;

	ptime t1(microsec_clock::local_time());

	for (;;) {
		shared_ptr<TSocket> socket(new TSocket(server, port));
		shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		KVServerClient client(protocol);
		transport->open();

		std::string key;

		for (int i = 0; i < 50; ++i) {
			key += '0' + (rand() % 10);
		}

		if (rand() % 2) {
			GetArgs args;
			args.key = key;
			GetReply reply;
			client.get(reply, args);
		}
		else {
			std::string value(10240, '0' + (rand() % 10));
			PutArgs args;
			args.key = key;
			args.value = value;
			PutReply reply;
			client.put(reply, args);
		}

		transport->close();

		++cnt;
		if (cnt % 100 == 0) {
			ptime t2(microsec_clock::local_time());
			double x = (t2 - t1).total_milliseconds();
			std::cout << cnt << " " << x / cnt << std::endl;
			t1 = t2;
		}
	}

	return 0;
}