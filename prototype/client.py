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

class ServerConnection:
  def __init__(self, server, port):
    socket = TSocket.TSocket(server, port)
    self.transport = TTransport.TFramedTransport(socket)
    self.transport.open()
    self.protocol = TBinaryProtocol.TBinaryProtocol(self.transport)

  def getProtocol(self):
    return self.protocol

  def close(self):
    self.transport.close()

class CacheClient:
  def __init__(self, server, port):
    connection = ServerConnection(server, port)
    client = ViewService.Client(connection.getProtocol())
    self.servers = client.getView().servers
    connection.close()
    print str(len(self.servers)) + " servers in charge"

  def get(self, key):
    try:
      serverID = self.getServerID(key)
      connection = ServerConnection(self.servers[serverID].server, self.servers[serverID].port)
      client = KVStorage.Client(connection.getProtocol())
      reply = client.get(GetArgs(key))
      connection.close()
      if (reply.status == Status.OK):
        print "Got " + key + " " + reply.value
        return reply.value
      print "Got " + key + " Nothing"
      return ""
    except: 
      raise Exception('Get failed')

  def put(self, key, val):
    try:
      serverID = self.getServerID(key)
      connection = ServerConnection(self.servers[serverID].server, self.servers[serverID].port)
      client = KVStorage.Client(connection.getProtocol())
      reply = client.put(PutArgs(key, val))
      connection.close()
      print "Put " + key + " " + val
    except: 
      raise Exception('Put failed')

  def getServerID(self, key):
    result = 0
    for c in key:
      result ^= ord(c)
      result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24)
    return result % len(self.servers)

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
    key = str(random.getrandbits(4))
    cacheClient.getServerID(key)
    if (bool(random.getrandbits(1))):
      value = str(random.getrandbits(10))
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
