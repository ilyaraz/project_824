#include "ServerConnection.h"
#include "ConsistentHashing.h"

#include <KVStorage.h>
#include <ViewService.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include <fstream>
#include <mutex>
#include <boost/thread.hpp>

const int WAIT_CHECK_TIME = 500;
const int DEAD_TIME = 500;

class ViewServiceHandler : virtual public ViewServiceIf {
 public:
  ViewServiceHandler() {
    viewMutex.lock();
    viewNum = 0;
    views.push_back(std::vector<Server>());
    viewMutex.unlock();

    boost::thread t1(boost::bind(&ViewServiceHandler::checkPings, this));
  }

  void getView(GetServersReply& _return) {
    viewMutex.lock();
    std::map<int, Server> servers;
    for (size_t i = 0; i < views[viewNum].size(); ++i) {
        servers[::hash(views[viewNum][i])] = views[viewNum][i];
    }
    _return.view.hashToServer = servers;
    _return.viewNum = viewNum;
    viewMutex.unlock();
  }

  void getStatistics(GetStatisticsReply& _return) {
      GetStatisticsReply result;
      viewMutex.lock();
      std::vector<Server> view = views[viewNum];
      viewMutex.unlock();
      for (size_t i = 0; i < view.size(); ++i) {
          try {
              ServerConnection<KVStorageClient> connection(view[i].server, view[i].port);
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

  struct ltstr {
    bool operator()(const Server s1, const Server s2) const {
      return s1.server.compare(s2.server) == -1 || (s1.server.compare(s2.server) == 0 && s1.port < s2.port);
    }
  };

  void addServer(const Server& s) {
    pingsMutex.lock();
    pings[s] = boost::posix_time::microsec_clock::local_time();
    pingsMutex.unlock();

    viewMutex.lock();
    std::vector<Server> newView = std::vector<Server>(views[viewNum]);
    newView.push_back(s);
    views.push_back(newView);
    viewNum++;
    std::cout << "Incrementing viewnum to " << viewNum << std::endl;
    viewMutex.unlock();
  }

  void receivePing(GetServersReply& _return, const Server& s) {
    std::cout << "received ping from " << s.server << ":" << s.port << std::endl;
    pingsMutex.lock();
    if (pings.find(s) != pings.end()) {
        pings[s] = boost::posix_time::microsec_clock::local_time();
    }
    pingsMutex.unlock();
    getView(_return);
  }

 private:
  int viewNum;
  std::vector<std::vector<Server> > views;
  std::mutex viewMutex;
  std::map<Server, boost::posix_time::ptime, ltstr> pings;
  std::mutex pingsMutex;

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

  void checkPings() {
    while (true) {
        std::cout << "entering" << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(WAIT_CHECK_TIME));
      viewMutex.lock();
      std::cout << "Current viewnumber is " << viewNum << std::endl;
      viewMutex.unlock();
      GetServersReply reply;
      getView(reply);
      for (std::map<int, Server>::iterator it = reply.view.hashToServer.begin(); it != reply.view.hashToServer.end(); it++) {
          std::cout << it->first << " " << it->second.server << " " << it->second.port << std::endl;
      }

      //Check if any servers are unresponsive
      std::vector<Server> toRemove = std::vector<Server>();
      std::map<Server, boost::posix_time::ptime>::iterator iter;
      pingsMutex.lock();
      boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
      for (iter = pings.begin(); iter != pings.end(); ++iter) {
        if (iter->second < now - boost::posix_time::milliseconds(DEAD_TIME)) {
          toRemove.push_back(iter->first);
          pings.erase(iter->first);
        }
      }
      pingsMutex.unlock();

      //If any servers are unresponsive, create a new view
      if (!toRemove.empty()) {
        viewMutex.lock();
        std::vector<Server> newView = std::vector<Server>();
        std::vector<Server> oldView = views[viewNum];
        for (size_t i = 0; i < oldView.size(); ++i) {
          if (std::find(toRemove.begin(), toRemove.end(), oldView[i]) == toRemove.end()) {
            newView.push_back(oldView[i]);
          }
        }
        views.push_back(newView);
        viewNum++;
        viewMutex.unlock();
      }
    }
  }
};

int main(int argc, char **argv) {
  int port = atoi(argv[1]);
  shared_ptr<ViewServiceHandler> handler(new ViewServiceHandler());
  shared_ptr<apache::thrift::TProcessor> processor(new ViewServiceProcessor(handler));
  shared_ptr<apache::thrift::protocol::TProtocolFactory> protocolFactory(new apache::thrift::protocol::TBinaryProtocolFactory());

  apache::thrift::server::TNonblockingServer server(processor, protocolFactory, port);
  server.serve();
  std::cout << "Blah" << std::endl;
  return 0;
}

