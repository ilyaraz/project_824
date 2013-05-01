#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>
#include "cache_types.h"

std::string hash(std::string msg) {
  unsigned char digest[MD5_DIGEST_LENGTH];
  char digest_str[2*MD5_DIGEST_LENGTH];
  std::string result;
  MD5((const unsigned char*) msg.c_str(), msg.length(), digest);
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    sprintf(digest_str, "%02x", digest[i]);
    result.append(digest_str);
  }
  std::cout << result << std::endl;
  return result;
}

void getServer(Server& _return, std::string hash, const View& v) {
  std::vector<std::string> sortedHashes = v.sortedHashes;
  int index = std::lower_bound(sortedHashes.begin(), sortedHashes.end(), hash) - sortedHashes.begin();
  std::map<std::string, Server> hashToServer = v.hashToServer;
  _return = hashToServer[sortedHashes[index]];
}
