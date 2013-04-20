struct PutArgs {
	1: string key,
	2: string value
}

struct GetArgs {
	1: string key
}

enum Status {
	OK = 1,
	NO_KEY = 2
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

struct GetServersReply {
	1: list<Server> servers
}

service KVStorage {
	PutReply put(1: PutArgs query),
	GetReply get(1: GetArgs query)
}

service ViewService {
	GetServersReply getView()
}
