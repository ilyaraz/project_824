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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <unistd.h>

const int WAIT_CHECK_TIME = 500;
const int DEAD_TIME = 500;

class ViewServiceHandler : virtual public ViewServiceIf {
 public:
  ViewServiceHandler() {
    gen.seed(getpid());
    viewMutex.lock();
    viewNum = 0;
    views.push_back(std::vector<Server>());
    hashToServer.push_back(std::map<int, std::vector<Server> >());
    viewMutex.unlock();

    boost::thread t1(boost::bind(&ViewServiceHandler::checkPings, this));

  }

  void getView(GetServersReply& _return) {
    viewMutex.lock();
    _return.view.hashToServer = hashToServer[viewNum];
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
      return make_pair(s1.server, s1.port) < make_pair(s2.server, s2.port);
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
    std::map<int, std::vector<Server> > newHash(hashToServer[viewNum]);
    newHash[gen()] = std::vector<Server>(1, s); 
    hashToServer.push_back(newHash);
    viewNum++;
    std::cout << "Incrementing viewnum to " << viewNum << std::endl;
    viewMutex.unlock();
  }

  void addReplica(const Server& s, const Server &master) {
    viewMutex.lock();
    bool ok = false;
    for (size_t i = 0; i < views[viewNum].size(); i++) {
        if (views[viewNum][i] == master) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        viewMutex.unlock();
        return;
    }
    std::vector<Server> newView = std::vector<Server>(views[viewNum]);
    newView.push_back(s);
    views.push_back(newView);
    std::map<int, std::vector<Server> > newHash(hashToServer[viewNum]);
    for (std::map<int, std::vector<Server> >::iterator it = hashToServer[viewNum].begin(); it != hashToServer[viewNum].end(); it++) {
        if (std::find(it->second.begin(), it->second.end(), master) != it->second.end()) {
            newHash[it->first].push_back(s);
            break;
        }
    }
    hashToServer.push_back(newHash);
    viewNum++;
    std::cout << "Incrementing viewnum to " << viewNum << std::endl;
    pingsMutex.lock();
    pings[s] = boost::posix_time::microsec_clock::local_time();
    pingsMutex.unlock();
    viewMutex.unlock();
  }

  void receivePing(GetServersReply& _return, const Server& s) {
    //std::cout << "received ping from " << s.server << ":" << s.port << std::endl;
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
  std::vector<std::map<int, std::vector<Server> > > hashToServer;
  std::mutex viewMutex;
  std::map<Server, boost::posix_time::ptime, ltstr> pings;
  std::mutex pingsMutex;
  boost::mt19937 gen;

  GetStatisticsReply aggregateStatistics(const GetStatisticsReply &a, const GetStatisticsReply &b) {
      GetStatisticsReply result;
      result.numInsertions = a.numInsertions + b.numInsertions;
      result.numUpdates = a.numUpdates + b.numUpdates;
      result.numGoodGets = a.numGoodGets + b.numGoodGets;
      result.numFailedGets = a.numFailedGets + b.numFailedGets;
      result.dataSize = a.dataSize + b.dataSize;
      result.memoryOverhead = a.memoryOverhead + b.memoryOverhead;
      result.numEvictions = a.numEvictions + b.numEvictions;
      result.numPurges = a.numPurges + b.numPurges;
      return result;
  }

  void checkPings() {
    while (true) {
        std::cout << "entering" << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(WAIT_CHECK_TIME));
      viewMutex.lock();
      std::cout << "Current viewnumber is " << viewNum << std::endl;
      viewMutex.unlock();
      for (std::map<int, std::vector<Server> >::iterator it = hashToServer[viewNum].begin(); it != hashToServer[viewNum].end(); it++) {
          std::cout << it->first << ": ";
          for (std::vector<Server>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
              std::cout << it2->server << ":" << it2->port << " (last seen " << pings[*it2] << ") ";
          }
          std::cout << std::endl;
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
        std::map<int, std::vector<Server> > newHash;
        for (size_t i = 0; i < oldView.size(); ++i) {
          if (std::find(toRemove.begin(), toRemove.end(), oldView[i]) == toRemove.end()) {
            newView.push_back(oldView[i]);
          }
        }
        for (std::map<int, std::vector<Server> >::iterator it = hashToServer[viewNum].begin(); it != hashToServer[viewNum].end(); ++it) {
          for (std::vector<Server>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
              if (std::find(toRemove.begin(), toRemove.end(), *it2) == toRemove.end()) {
                  newHash[it->first].push_back(*it2);
              }
          }
        }
        hashToServer.push_back(newHash);
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

