#include <ViewService.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include <fstream>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

class ViewServiceHandler : virtual public ViewServiceIf {
 public:
  ViewServiceHandler(const std::vector<Server> &servers): servers(servers) {
  }

  void getView(GetServersReply& _return) {
    _return.servers = servers;
  }

 private:
  std::vector<Server> servers;

};

int main(int argc, char **argv) {
  int port = atoi(argv[1]);
  std::ifstream input(argv[2]);
  std::vector<Server> servers;
  std::string ss;
  int pp;
  while (input >> ss >> pp) {
      Server s;
      s.server = ss;
      s.port = pp;
      servers.push_back(s);
  }
  std::cout << servers.size() << " servers loaded" << std::endl;
  shared_ptr<ViewServiceHandler> handler(new ViewServiceHandler(servers));
  shared_ptr<TProcessor> processor(new ViewServiceProcessor(handler));
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TNonblockingServer server(processor, protocolFactory, port);
  server.serve();
  return 0;
}

