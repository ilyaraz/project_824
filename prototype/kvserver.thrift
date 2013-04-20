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

service KVServer {
	PutReply put(1: PutArgs query),
	GetReply get(1: GetArgs query)
}
