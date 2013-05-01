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

    Record(): state(EMPTY), prev(-1), next(-1) {
    }
};

const double LOAD_FACTOR1 = 0.25;
const double LOAD_FACTOR2 = 0.5;
const double GROWTH_FACTOR = 2.0;
const int MEMORY_LIMIT = 1 << 30;
const int PING_INTERVAL = 100; // in milliseconds

class KVStorageHandler: virtual public KVStorageIf {
public:
	KVStorageHandler(const std::string &address, const int &port, const std::string &vsAddress, const int &vsPort): hashTable(10), listHead(-1), listTail(-1), totalKeyValueSize(0), numEntries(0), numDeadEntries(0), vsAddress(vsAddress), vsPort(vsPort) {
          server.server = address;
          server.port = port;
          ServerConnection<ViewServiceClient> vsConnection(vsAddress, vsPort);
          vsConnection.getClient()->addServer(server);
          boost::thread t1(boost::bind(&KVStorageHandler::pingViewService, this));
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
            totalKeyValueSize += hashTable[pos].value.size();
            ++statistics.numUpdates;
            accessEntry(pos);
        }
        else {
            ++statistics.numInsertions;
            addEntry(pos, query.key, query.value);
        }
		_return.status = Status::OK;
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

    void cleanHash() {
        hashMutex.lock();
        for (int pos = listHead; pos != -1; pos = hashTable[pos].next) {
            Server supposedServer = getServer(::hash(hashTable[pos].key), currentView.view);
            if (supposedServer.server == server.server && supposedServer.port == server.port) {
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
        Server supposedServer = getServer(::hash(key), _currentView.view);
        return supposedServer.server == server.server && supposedServer.port == server.port;
    }

    void debug() {
        std::cout << getUsedMemory() << std::endl;
        for (int i = listHead; i != -1; i = hashTable[i].next) {
            std::cout << "(" << hashTable[i].key << ", " << hashTable[i].value << ") ";
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

        addToFront(pos);

        ++numEntries;
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

	shared_ptr<KVStorageHandler> handler(new KVStorageHandler(address, port, vsAddress, vsPort));
	shared_ptr<TProcessor> processor(new KVStorageProcessor(handler));
	//shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	//shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	//TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    TNonblockingServer server(processor, protocolFactory, port);
	server.serve();
	return 0;
}
