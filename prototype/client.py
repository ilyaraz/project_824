#!/usr/bin/env python

import random
import sys
sys.path.append('./gen-py')
sys.path.append('/usr/lib/python2.7/site-packages')

from cache import KVStorage, ViewService
from cache.ttypes import *

from thrift import Thrift
from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol

class CacheClient:
  def __init__(self, server, port):
#    try:
    client = ViewService.Client(CacheClient.getProtocol(server, port))
    self.servers = client.getView().servers
    print str(len(servers)) + " servers in charge\n"
#    except:
#      raise Exception('Failed to query view server')
      

  def get(self, key):
    try:
      serverID = self.getServerID(key)
      client = KVStorageClient.Client(getProtocol(self.servers[serverID].server, self.servers[serverID].port))
      reply = client.get(GetArgs(key))
      if (reply.status == Status.OK):
        return reply.value
      return ""
    except: 
      raise Exception('Get failed')

  def put(self, key, val):
    try:
      serverID = self.getServerID(key)
      client = KVStorageClient.Client(getProtocol(self.servers[serverID].server, self.servers[serverID].port))
      reply = client.put(PutArgs(key, val))
    except: 
      raise Exception('Put failed')

  def getServerID(self, key):
    result = 0
    for c in key:
      result ^= int(c)
      result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24)
    return result % len(self.servers)

  @staticmethod
  def getProtocol(server, port):
    transport = TSocket.TSocket(server, port)
    transport = TTransport.TFramedTransport(transport)
    return TBinaryProtocol.TBinaryProtocol(transport)


def main(args):
  random.seed()
  if (len(args) < 3):
    print 'Please provide a server and a port'
    return
  cacheClient = CacheClient(args[1], int(args[2]))

  cnt = 0
  numFailedPuts = 0
  numFailedGets = 0

  while True:
    key = str(random.getrandbits(32))
    cacheClient.getServerID(key)
    if (bool(random.getrandbits(1))):
      value = str(random.getrandbits(65536))
      try:
        cacheClient.put(key, value)
      except:
        numFailedPuts += 1
    else:
      try: 
        cacheClient.get(key)
      except:
        numFailedGets += 1

if __name__ == "__main__":
  main(sys.argv)
