
struct PutArgs {
	1: string key,
	2: string value,
        3: i32 viewNum
}

struct GetArgs {
	1: string key,
        2: i32 viewNum
}

enum Status {
	OK = 1,
	NO_KEY = 2,
        WRONG_SERVER = 3
}

struct PutReply {
	1: Status status
}

struct GetReply {
	1: Status status,
	2: string value
}

struct Server {
	1: string server,
	2: i32 port
}

struct View {
  1: map<i32, list<Server> > hashToServer
}

struct GetServersReply {
	1: View view, 
        2: i32 viewNum
}


struct GetStatisticsReply {
    1: i64 numInsertions,
    2: i64 numUpdates,
    3: i64 numGoodGets,
    4: i64 numFailedGets,
    5: i64 dataSize,
    6: i64 memoryOverhead,
    7: i64 numEvictions,
    8: i64 numPurges
}

struct VersionEntry {
    1: Server server
    2: i64 version
}

struct Entry {
    1: string key,
    2: string value,
    3: list<VersionEntry> version
}

struct AntiEntropyArgs {
    1: list<Entry> entries
    2: i32 viewNum
}

struct AntiEntropyReply {
    1: Status status,
    2: list<string> invalidatedEntries
}

service KVStorage {
	PutReply put(1: PutArgs query),
	GetReply get(1: GetArgs query),
    GetStatisticsReply getStatistics(),
    AntiEntropyReply antiEntropy(1: AntiEntropyArgs data)
}

service ViewService {
        void addServer(1: Server s),
        void addReplica(1: Server s, 2: Server master),
        GetServersReply receivePing(1: Server s),
	GetServersReply getView(),
    GetStatisticsReply getStatistics()
}
