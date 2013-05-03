#!/usr/bin/env python

import random
import sys
import cPickle
from bisect import bisect
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
    self.vsServer = server
    self.vsPort = port
    self.getView()


  def getView(self):
    connection = ServerConnection(self.vsServer, self.vsPort)
    client = ViewService.Client(connection.getProtocol())
    reply = client.getView()
    self.view = reply.view
    self.viewNum = reply.viewNum
    connection.close()

  def get(self, key):
    try:
      server = self.getServer(key)
      connection = ServerConnection(server.server, server.port)
      client = KVStorage.Client(connection.getProtocol())
      reply = client.get(GetArgs(key, self.viewNum))
      connection.close()
      if (reply.status == Status.OK):
        #print "Got " + key + " " + reply.value
        unpickledVal = cPickle.loads(reply.value)
        return unpickledVal
      elif (reply.status == Status.NO_KEY):
        #print "Got " + key + " Nothing"
        return ""
      else:
        self.getView()
        raise Exception('Wrong server') 
    except:
      raise Exception('Get failed')

  def put(self, key, val):
    try: 
      server = self.getServer(key)
      connection = ServerConnection(server.server, server.port)
      client = KVStorage.Client(connection.getProtocol())
      pickledVal = cPickle.dumps(val)
      reply = client.put(PutArgs(key, pickledVal, self.viewNum))
      connection.close()
      if (reply.status == Status.WRONG_SERVER):
        self.getView()
        raise Exception('Wrong server')
      #print "Put " + key + " " + val
    except:
      raise Exception('Put failed')

  def getServer(self, key):
    result = 0
    for c in key:
      result ^= convert(ord(c))
      result += convert((convert(result) << 1) + (convert(result) << 4) + (convert(result) << 7) + (convert(result) << 8) + (convert(result) << 24))
    result = result % 4294967296
    if result > 2147483648:
      result = result - 4294967295 
    sortedKeys = sorted(self.view.hashToServer.keys())
    pos = bisect(sortedKeys, result)
    if pos == len(sortedKeys):
      key = sortedKeys[0]
    else:
      key = sortedKeys[pos]
    return random.choice(self.view.hashToServer[key])

def convert(n):
  return (n & 0xFFFFFFFF)

def main(args):
  random.seed()
  if (len(args) < 3):
    print 'Please provide a server and a port'
    return
  cacheClient = CacheClient(args[1], int(args[2]))

  cnt = 0
  numFailedPuts = 0
  numFailedGets = 0

  key = "asdf"
  val = ""
  cacheClient.put(key,val)

  while True:
    key = str(random.getrandbits(9))
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
