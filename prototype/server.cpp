#include "ServerConnection.h"
#include "ConsistentHashing.h"

#include <KVStorage.h>
#include <ViewService.h>

#include <cassert>

#include <thrift/protocol/TBinaryProtocol.h>
//#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include <boost/thread.hpp>
#include <mutex>

#include <iostream>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using boost::shared_ptr;

enum State {
    EMPTY,
    ALIVE,
    DEAD
};

struct Record {
    State state; // 0 -- empty, 1 -- alive, 2 -- dead
    std::string key;
    std::string value;
    int prev;
    int next;

    std::vector<std::pair<Server, long long> > version;

    Record(): state(EMPTY), prev(-1), next(-1) {
    }
};

const double LOAD_FACTOR1 = 0.25;
const double LOAD_FACTOR2 = 0.5;
const double GROWTH_FACTOR = 2.0;
const int MEMORY_LIMIT = 1 << 30;
const int PING_INTERVAL = 100; // in milliseconds
const int ANTI_ENTROPY_WAIT = 1000;

class KVStorageHandler: virtual public KVStorageIf {
public:
	KVStorageHandler(const std::string &address, const int &port, const std::string &vsAddress, const int &vsPort,
            const std::string &masterAddress, const int &masterPort): hashTable(10), listHead(-1), listTail(-1), totalKeyValueSize(0), numEntries(0), numDeadEntries(0), vsAddress(vsAddress), vsPort(vsPort) {
          server.server = address;
          server.port = port;
          ServerConnection<ViewServiceClient> vsConnection(vsAddress, vsPort);
          if (masterAddress == "") {
              vsConnection.getClient()->addServer(server);
          }
          else {
              Server master;
              master.server = masterAddress;
              master.port = masterPort;
              vsConnection.getClient()->addReplica(server, master);
          }
          boost::thread t1(boost::bind(&KVStorageHandler::pingViewService, this));
          boost::thread t2(boost::bind(&KVStorageHandler::antiEntropyHandler, this));
        }

        void printVersion(std::vector<std::pair<Server, long long> > &v) {
            std::cout << "(";
            for (size_t i = 0; i < v.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << v[i].first.server << ":" << v[i].first.port << "->" << v[i].second;
            }
            std::cout << ")";
        }

        bool atMost(std::vector<std::pair<Server, long long> > &v1, std::vector<std::pair<Server, long long> > &v2) {
            for (size_t i = 0; i < v1.size(); ++i) {
                for (size_t j = 0; j < v2.size(); ++j) {
                    if (v1[i].first == v2[j].first && v1[i].second > v2[j].second) {
                        return false;
                    }
                }
            }
            for (size_t i = 0; i < v1.size(); ++i) {
                bool ok = false;
                for (size_t j = 0; j < v2.size(); ++j) {
                    if (v1[i].first == v2[j].first) {
                        ok = true;
                        break;
                    }
                }
                if (!ok) return false;
            }
            return true;
        }

        void updateVersion(std::vector<std::pair<Server, long long> > &v1, const std::pair<Server, long long> &v2) {
            for (size_t i = 0; i < v1.size(); ++i) {
                if (v2.first == v1[i].first) {
                    v1[i].second = std::max(v1[i].second, v2.second);
                }
            }
        }
        
        std::vector<std::pair<Server, long long> > maximumVersion(std::vector<std::pair<Server, long long> > &v1, std::vector<std::pair<Server, long long> > &v2) {
            std::vector<std::pair<Server, long long> > result;
            for (size_t i = 0; i < v1.size(); ++i) {
                updateVersion(result, v1[i]);
            }
            for (size_t i = 0; i < v2.size(); ++i) {
                updateVersion(result, v2[i]);
            }
            return result;
        }

        void propagate(const Entry &entry, std::vector<std::string> &invalidatedEntries) {
            normalizeTable();
            int pos = getPosition(entry.key);
            if (hashTable[pos].state == ALIVE) {
                std::vector<std::pair<Server, long long> > v(entry.version.size());
                for (size_t i = 0; i < v.size(); ++i) {
                    v[i] = std::make_pair(entry.version[i].server, entry.version[i].version);
                }
                //printVersion(v);
                //std::cout << " vs ";
                //printVersion(hashTable[pos].version);
                //std::cout << std::endl;

                if (atMost(v, hashTable[pos].version)) {
                    //std::cout << "skipping" << std::endl;
                    return;
                }
                if (atMost(hashTable[pos].version, v)) {
                    //std::cout << "replacing" << std::endl;
                    totalKeyValueSize -= hashTable[pos].value.size();
                    hashTable[pos].value = entry.value;
                    totalKeyValueSize += hashTable[pos].value.size();
                    accessEntry(pos);
                    hashTable[pos].version.swap(v);
                    return;
                }
                if (entry.value == hashTable[pos].value) {
                    //std::cout << "conflict resolved" << std::endl;
                    hashTable[pos].version = maximumVersion(v, hashTable[pos].version);
                    return;
                }
                //std::cout << "conflict: invalidating entries" << std::endl;
                invalidatedEntries.push_back(hashTable[pos].key);
                removeEntry(pos);
            }
            else {
                addEntry(pos, entry.key, entry.value, entry.version);
            }
        }

        void antiEntropy(AntiEntropyReply& _return, const AntiEntropyArgs& query) {
            currentViewMutex.lock();
            bool ok = currentView.viewNum == query.viewNum;
            currentViewMutex.unlock();
            if (!ok) {
                _return.status = Status::WRONG_SERVER;
                return;
            }
            hashMutex.lock();
            for (auto it = query.entries.begin(); it != query.entries.end(); ++it) {
                propagate(*it, _return.invalidatedEntries);
            }
            hashMutex.unlock();
            _return.status = Status::OK;
        }

        void antiEntropyHandler() {
            for (;;) {
                boost::this_thread::sleep(boost::posix_time::milliseconds(ANTI_ENTROPY_WAIT));
                hashMutex.lock();
                for (auto it = diffs.begin(); it != diffs.end(); ++it) {
                    std::cout << it->first.server << ":" << it->first.port << " -> " << it->second.size() << std::endl;
                }
                std::cout << "----------" << std::endl;
                for (auto it = diffs.begin(); it != diffs.end(); ++it) {
                    if (!it->second.empty()) {
                        currentViewMutex.lock();
                        AntiEntropyArgs args;
                        args.viewNum = currentView.viewNum; 
                        currentViewMutex.unlock();
                        for (auto it1 = it->second.begin(); it1 != it->second.end(); ++it1) {
                            int pos = getPosition(*it1);
                            if (hashTable[pos].state != ALIVE) {
                                continue;
                            }
                            Entry entry;
                            entry.key = hashTable[pos].key;
                            entry.value = hashTable[pos].value;
                            entry.version.resize(hashTable[pos].version.size());
                            for (size_t i = 0; i < hashTable[pos].version.size(); ++i) {
                                entry.version[i].server = hashTable[pos].version[i].first;
                                entry.version[i].version = hashTable[pos].version[i].second;
                            }
                            args.entries.push_back(entry);
                        }
                        AntiEntropyReply reply;
                        hashMutex.unlock();
                        try {
                            ServerConnection<KVStorageClient> connection(it->first.server, it->first.port);
                            connection.getClient()->antiEntropy(reply, args);
                        }
                        catch (...) {
                            reply.status = Status::WRONG_SERVER;
                        }
                        hashMutex.lock();
                        if (reply.status == Status::OK) {
                            diffs.erase(it);
                            std::cout << "invalidating " << reply.invalidatedEntries.size() << " entries" << std::endl;
                            for (auto it = reply.invalidatedEntries.begin(); it != reply.invalidatedEntries.end(); ++it) {
                                int pos = getPosition(*it);
                                if (hashTable[pos].state != ALIVE) {
                                    continue;
                                }
                                removeEntry(pos);
                            }
                        }
                        break;
                    }
                }
                hashMutex.unlock();
            }
        }

        void pingViewService() {
          while (true) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(PING_INTERVAL));
            try {
                ServerConnection<ViewServiceClient> vsConnection(vsAddress, vsPort);
                GetServersReply _currentView;
                vsConnection.getClient()->receivePing(_currentView, server);
                currentViewMutex.lock();
                bool needToClean = false;
                if (currentView.viewNum != _currentView.viewNum) {
                    needToClean = true;
                }
                currentView = _currentView;
                currentViewMutex.unlock();
                if (needToClean) {
                    cleanHash();
                }
            }
            catch (...) {
            }
          }
        }

	void put(PutReply& _return, const PutArgs& query) {
        if (!checkServer(query.viewNum, query.key)) {
            _return.status = Status::WRONG_SERVER;
            return;
        }
        hashMutex.lock();
        //debug();
        normalizeTable();
        //debug();
        int pos = getPosition(query.key);
        if (hashTable[pos].state == ALIVE) {
            totalKeyValueSize -= hashTable[pos].value.size();
            hashTable[pos].value = query.value;
            bool ok = false;
            for (std::vector<std::pair<Server, long long> >::iterator it = hashTable[pos].version.begin(); it != hashTable[pos].version.end(); ++it) {
                if (it->first == server) {
                    ++it->second;
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                hashTable[pos].version.push_back(std::make_pair(server, 1));
            }
            totalKeyValueSize += hashTable[pos].value.size();
            ++statistics.numUpdates;
            accessEntry(pos);
        }
        else {
            ++statistics.numInsertions;
            addEntry(pos, query.key, query.value);
        }
		_return.status = Status::OK;
        currentViewMutex.lock();
        std::vector<Server> servers = getServer(ir_hash(query.key), currentView.view);
        currentViewMutex.unlock();
        for (size_t i = 0; i < servers.size(); ++i) {
            if (servers[i] == server) {
                continue;
            }
            diffs[servers[i]].insert(query.key);
        }
        hashMutex.unlock();
        //debug();
	}

	void get(GetReply& _return, const GetArgs& query) {
        if (!checkServer(query.viewNum, query.key)) {
            _return.status = Status::WRONG_SERVER;
            return;
        }
        //debug();
        hashMutex.lock();
        int pos = getPosition(query.key);
        if (hashTable[pos].state == ALIVE) {
            _return.status = Status::OK;
            _return.value = hashTable[pos].value;
            ++statistics.numGoodGets;
            accessEntry(pos);
        }
        else {
            _return.status = Status::NO_KEY;
            ++statistics.numFailedGets;
        }
        hashMutex.unlock();
        //debug();
	}

    void getStatistics(GetStatisticsReply& _return) {
        statistics.dataSize = getUsedMemory();
        statistics.memoryOverhead = sizeof(Record) * hashTable.size();
        _return = statistics;
    }

private:
    GetStatisticsReply statistics;
    std::vector<Record> hashTable;
    int listHead;
    int listTail;
    int totalKeyValueSize;
    int numEntries;
    int numDeadEntries;
    Server server;
    GetServersReply currentView;
    std::mutex currentViewMutex;
    std::string vsAddress;
    int vsPort;
    std::mutex hashMutex;

    struct cmpServers {
        bool operator()(const Server &a, const Server &b) const {
            return std::make_pair(a.server, a.port) < std::make_pair(b.server, b.port);
        }
    };

    std::map<Server, std::set<std::string>, cmpServers> diffs;


    void adjustVersion(std::vector<std::pair<Server, long long> > &version, const std::vector<Server> &servers) {
        std::vector<std::pair<Server, long long> > newV;
        for (std::vector<std::pair<Server, long long> >::iterator it = version.begin(); it != version.end(); ++it) {
            if (std::find(servers.begin(), servers.end(), it->first) != servers.end()) {
                newV.push_back(*it);
            }
        }
        version.swap(newV);
        for (size_t i = 0; i < servers.size(); ++i) {
            if (!diffs.count(servers[i]) && servers[i] != server) {
                for (int pos = listHead; pos != -1; pos = hashTable[pos].next) {
                    diffs[servers[i]].insert(hashTable[pos].key);
                }
            }
        }
        for (auto it = diffs.begin(); it != diffs.end(); ++it) {
            if (std::find(servers.begin(), servers.end(), it->first) == servers.end()) {
                diffs.erase(it);
            }
        }
    }

    void cleanHash() {
        hashMutex.lock();
        for (int pos = listHead; pos != -1; pos = hashTable[pos].next) {
            std::vector<Server> supposedServer = getServer(ir_hash(hashTable[pos].key), currentView.view);
            if (std::find(supposedServer.begin(), supposedServer.end(), server) != supposedServer.end()) {
                adjustVersion(hashTable[pos].version, supposedServer);
                continue;
            }
            removeEntry(pos);
            ++statistics.numPurges;
        }
        hashMutex.unlock();
    }

    bool checkServer(int viewNum, const std::string &key) {
        currentViewMutex.lock();
        GetServersReply _currentView = currentView;
        currentViewMutex.unlock();
        if (viewNum < _currentView.viewNum) {
            return false;
        }
        if (viewNum > _currentView.viewNum) {
            ServerConnection<ViewServiceClient> vsConnection(vsAddress, vsPort);
            vsConnection.getClient()->receivePing(_currentView, server);
            currentViewMutex.lock();
            bool needToClean = false;
            if (currentView.viewNum != _currentView.viewNum) {
                needToClean = true;
            }
            currentView = _currentView;
            currentViewMutex.unlock();
            if (needToClean) {
                cleanHash();
            }
            if (viewNum != _currentView.viewNum) {
                return false;
            }
        }
        std::vector<Server> supposedServer = getServer(ir_hash(key), _currentView.view);
        return std::find(supposedServer.begin(), supposedServer.end(), server) != supposedServer.end(); 
    }

    void debug() {
        std::cout << getUsedMemory() << std::endl;
        for (int i = listHead; i != -1; i = hashTable[i].next) {
            std::cout << "(" << hashTable[i].key << ", " << hashTable[i].value;
            for (size_t j = 0; j < hashTable[i].version.size(); ++j) {
                std::cout << ", " << hashTable[i].version[j].first.server << ":" << hashTable[i].version[j].first.port << "->" << hashTable[i].version[j].second;
            }
            std::cout << ") ";
        }
        std::cout << std::endl;
    }

    int getPosition(const std::string &key) {
        unsigned int pos = hash(key) % hashTable.size();
        for (;;) {
            if (hashTable[pos].state == EMPTY) return pos;
            if (hashTable[pos].state == ALIVE && hashTable[pos].key == key) return pos;
            pos++;
            if (pos >= hashTable.size()) pos = 0;
        }
    }

    unsigned int hash(const std::string &key) {
        unsigned int result = 0;
        for (size_t i = 0; i < key.size(); ++i) {
            result ^= (unsigned int) key[i];
            result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
        }
        return result;
    }

    void rehash(size_t newSize) {
        std::vector<Record> newTable(newSize);
        std::cout << "rehash from " << hashTable.size() << " to " << newTable.size() << std::endl;
        int newListHead = -1, newListTail = -1;
        for (int i = listHead; i != -1; i = hashTable[i].next) {
            //std::cout << "rehashing " << i << std::endl;
            unsigned int pos = hash(hashTable[i].key) % newTable.size();
            for (;;) {
                if (newTable[pos].state == EMPTY) break;
                pos++;
                if (pos >= newTable.size()) pos = 0;
            }
            newTable[pos].state = ALIVE;
            newTable[pos].key.swap(hashTable[i].key);
            newTable[pos].value.swap(hashTable[i].value);
            newTable[pos].version.swap(hashTable[i].version);
            newTable[pos].prev = newListTail;
            if (newListTail != -1) {
                newTable[newListTail].next = pos;
            }
            newTable[pos].next = -1;
            if (newListHead == -1) {
                newListHead = pos;
            }
            newListTail = pos;
        }
        hashTable.swap(newTable);
        std::swap(listHead, newListHead);
        std::swap(listTail, newListTail);
        numDeadEntries = 0;
    }

    void removeEntry(int pos) {
        removeFromList(pos);
        totalKeyValueSize -= hashTable[pos].key.size() + hashTable[pos].value.size();
        Record emptyRecord;
        std::swap(hashTable[pos], emptyRecord);
        hashTable[pos].state = DEAD;
        std::vector<std::pair<Server, long long> > version;
        hashTable[pos].version.swap(version);
        numEntries--;
        numDeadEntries++;
    }

    void evictEntry() {
        assert(listTail != -1);
        removeEntry(listTail);
        ++statistics.numEvictions;
    }

    void normalizeTable() {
        //std::cout << numEntries << " " << numDeadEntries << " " << hashTable.size() << std::endl;
        while (getUsedMemory() > MEMORY_LIMIT) {
            evictEntry();
        }
        if (numEntries > LOAD_FACTOR1 * hashTable.size()) {
            rehash(hashTable.size() * GROWTH_FACTOR);
        }
        if (numEntries + numDeadEntries > LOAD_FACTOR2 * hashTable.size()) {
            rehash(hashTable.size());
        }
    }

    void removeFromList(int pos) {
        assert(hashTable[pos].state == ALIVE);
        if (hashTable[pos].prev != -1) {
            hashTable[hashTable[pos].prev].next = hashTable[pos].next;
        }
        else {
            listHead = hashTable[pos].next;
        }
        if (hashTable[pos].next != -1) {
            hashTable[hashTable[pos].next].prev = hashTable[pos].prev;
        }
        else {
            listTail = hashTable[pos].prev;
        }
    }

    void addToFront(int pos) {
        assert(hashTable[pos].state == ALIVE);
        hashTable[pos].prev = -1;
        hashTable[pos].next = listHead;

        if (listHead != -1) {
            hashTable[listHead].prev = pos;
        }

        listHead = pos;

        if (listTail == -1) {
            listTail = pos;
        }
    }

    void accessEntry(int pos) {
        assert(hashTable[pos].state == ALIVE);
        removeFromList(pos);
        addToFront(pos);
    }

    void addEntry(int pos, const std::string &key, const std::string &value) {
        assert(hashTable[pos].state == EMPTY);

        hashTable[pos].state = ALIVE;
        totalKeyValueSize += key.size() + value.size();
        hashTable[pos].key = key;
        hashTable[pos].value = value;

        std::vector<std::pair<Server, long long> > version(1, std::make_pair(server, 1));

        hashTable[pos].version.swap(version);

        addToFront(pos);

        ++numEntries;
    }

    void addEntry(int pos, const std::string &key, const std::string &value, const std::vector<VersionEntry> &version) {
        addEntry(pos, key, value);

        std::vector<std::pair<Server, long long> > version_(version.size());
        for (size_t i = 0; i < version.size(); ++i) {
            version_[i] = std::make_pair(version[i].server, version[i].version);
        }

        hashTable[pos].version.swap(version_);
    }

    int getUsedMemory() {
        return /*sizeof(hashTable) + hashTable.size() * sizeof(Record) + */totalKeyValueSize;
    }
};

int main(int argc, char **argv) {
        std::string address = argv[1];
	int port = atoi(argv[2]);
        std::string vsAddress = argv[3];
        int vsPort = atoi(argv[4]);

        std::string masterAddress = "";
        int masterPort = -1;
        if (argc > 5) {
            masterAddress = argv[5];
            masterPort = atoi(argv[6]); 
        }

	shared_ptr<KVStorageHandler> handler(new KVStorageHandler(address, port, vsAddress, vsPort, masterAddress, masterPort));
	shared_ptr<TProcessor> processor(new KVStorageProcessor(handler));
	//shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	//shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	//TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    TNonblockingServer server(processor, protocolFactory, port);
	server.serve();
	return 0;
}
