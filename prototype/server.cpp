#include <KVStorage.h>

#include <cassert>

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

struct Record {
    bool alive;
    std::string key;
    std::string value;
    int prev;
    int next;

    Record(): alive(false), prev(-1), next(-1) {
    }
};

const double LOAD_FACTOR = 0.5;
const double GROWTH_FACTOR = 2.0;
const int MEMORY_LIMIT = 1 << 30;

class KVStorageHandler: virtual public KVStorageIf {
public:
	KVStorageHandler(): hashTable(10), listHead(-1), listTail(-1), totalKeyValueSize(0), numEntries(0) {
    }

	void put(PutReply& _return, const PutArgs& query) {
        //debug();
        normalizeTable();
        //debug();
        int pos = getPosition(query.key);
        if (hashTable[pos].alive) {
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
        //debug();
	}

	void get(GetReply& _return, const GetArgs& query) {
        //debug();
        int pos = getPosition(query.key);
        if (hashTable[pos].alive) {
            _return.status = Status::OK;
            _return.value = hashTable[pos].value;
            ++statistics.numGoodGets;
            accessEntry(pos);
        }
        else {
            _return.status = Status::NO_KEY;
            ++statistics.numFailedGets;
        }
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
            if (!hashTable[pos].alive) return pos;
            if (hashTable[pos].key == key) return pos;
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

    void rehash() {
        std::vector<Record> newTable(hashTable.size() * GROWTH_FACTOR);
        std::cout << "rehash from " << hashTable.size() << " to " << newTable.size() << std::endl;
        int newListHead = -1, newListTail = -1;
        for (int i = listHead; i != -1; i = hashTable[i].next) {
            //std::cout << "rehashing " << i << std::endl;
            unsigned int pos = hash(hashTable[i].key) % newTable.size();
            for (;;) {
                if (!newTable[pos].alive) break;
                pos++;
                if (pos >= newTable.size()) pos = 0;
            }
            newTable[pos].alive = true;
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
    }

    void evictEntry() {
        assert(listTail != -1);
        int pos = listTail;
        //std::cout << "evicting " << hashTable[pos].key << " (load: " << (numEntries + 0.0) / (hashTable.size() + 0.0) << ")" << std::endl;
        removeFromList(pos);
        totalKeyValueSize -= hashTable[pos].key.size() + hashTable[pos].value.size();
        Record emptyRecord;
        std::swap(hashTable[pos], emptyRecord);
        numEntries--;
        ++statistics.numEvictions;
    }

    void normalizeTable() {
        while (getUsedMemory() > MEMORY_LIMIT) {
            evictEntry();
        }
        if (numEntries > LOAD_FACTOR * hashTable.size()) {
            rehash();
        }
    }

    void removeFromList(int pos) {
        assert(hashTable[pos].alive);
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
        assert(hashTable[pos].alive);
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
        assert(hashTable[pos].alive);
        removeFromList(pos);
        addToFront(pos);
    }

    void addEntry(int pos, const std::string &key, const std::string &value) {
        assert(!hashTable[pos].alive);

        hashTable[pos].alive = true;
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
