#include "ServerConnection.h"

#include <KVStorage.h>
#include <ViewService.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include <fstream>

class ViewServiceHandler : virtual public ViewServiceIf {
 public:
  ViewServiceHandler(const std::vector<Server> &servers): servers(servers) {
  }

  void getView(GetServersReply& _return) {
    _return.servers = servers;
  }

  void getStatistics(GetStatisticsReply& _return) {
      GetStatisticsReply result;
      for (size_t i = 0; i < servers.size(); ++i) {
          try {
              ServerConnection<KVStorageClient> connection(servers[i].server, servers[i].port);
              GetStatisticsReply local;
              connection.getClient()->getStatistics(local);
              result = aggregateStatistics(result, local);
          }
          catch (...) {
              std::cout << "server failed" << std::endl;
          }
      }
      _return = result;
  }

 private:
  std::vector<Server> servers;

  GetStatisticsReply aggregateStatistics(const GetStatisticsReply &a, const GetStatisticsReply &b) {
      GetStatisticsReply result;
      result.numInsertions = a.numInsertions + b.numInsertions;
      result.numUpdates = a.numUpdates + b.numUpdates;
      result.numGoodGets = a.numGoodGets + b.numGoodGets;
      result.numFailedGets = a.numFailedGets + b.numFailedGets;
      result.dataSize = a.dataSize + b.dataSize;
      result.memoryOverhead = a.memoryOverhead + b.memoryOverhead;
      result.numEvictions = a.numEvictions + b.numEvictions;
      return result;
  }

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
  shared_ptr<apache::thrift::TProcessor> processor(new ViewServiceProcessor(handler));
  shared_ptr<apache::thrift::protocol::TProtocolFactory> protocolFactory(new apache::thrift::protocol::TBinaryProtocolFactory());

  apache::thrift::server::TNonblockingServer server(processor, protocolFactory, port);
  server.serve();
  std::cout << "Blah" << std::endl;
  return 0;
}

