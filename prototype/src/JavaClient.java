import cache.*;

import org.apache.thrift.TException;
import org.apache.thrift.transport.TTransport;
import org.apache.thrift.transport.TFramedTransport;
import org.apache.thrift.transport.TSocket;
import org.apache.thrift.protocol.TProtocol;
import org.apache.thrift.protocol.TBinaryProtocol;

import java.util.List;
import java.util.Random;

class ServerConnection {
  private TSocket socket;
  private TFramedTransport transport;
  private TProtocol protocol;

  public ServerConnection(String host, int port) throws Exception {
    try {
      socket = new TSocket(host, port);
      transport = new TFramedTransport(socket);
      transport.open();
      protocol = new TBinaryProtocol(transport);
    } catch (Exception x) {
      throw new Exception("Failed to make a connection");
    }
  }

  public TProtocol getProtocol() {
    return protocol;
  }

  public void close() {
    socket.close();
    transport.close();
  }
}

class CacheClient {
  private List<Server> servers;
  public CacheClient(String host, int port) throws Exception {
    try {
      ServerConnection connection = new ServerConnection(host, port);
      ViewService.Client client = new ViewService.Client(connection.getProtocol());
      servers = client.getView().servers;
      connection.close();
      System.out.println("Servers in charge: " + servers.size());
    } catch (TException x) {
      throw new Exception("Failed to create Client");
    }
  }

  public String get(String key) throws Exception {
    ServerConnection connection = null;
    try {
      int serverID = getServerID(key);
      Server server = servers.get(serverID);
      connection = new ServerConnection(server.server, server.port);
      KVStorage.Client client = new KVStorage.Client(connection.getProtocol());
      GetReply getReply = client.get(new GetArgs(key));
      connection.close();
      if (getReply.status == Status.OK) {
        System.out.println("Get " + key + " " + getReply.value);
        return getReply.value;
      }
      System.out.println("Get " + key + " Nil");
      return "";
    } catch (TException x) {
      if (connection != null) {
        connection.close();
      }
      throw new Exception("Get failed");
    }
  }

  public void put(String key, String val) throws Exception {
    ServerConnection connection = null;
    try {
      int serverID = getServerID(key);
      Server server = servers.get(serverID);
      connection = new ServerConnection(server.server, server.port);
      KVStorage.Client client = new KVStorage.Client(connection.getProtocol());
      PutReply putReply = client.put(new PutArgs(key, val));
      connection.close();
      System.out.println("Put " + key + " " + val);
    } catch (TException x) {
      if (connection != null) {
        connection.close();
      }
      throw new Exception("Put failed");
    }
  }

  private int getServerID(String key) {
    int result = 0;
    for (char ch : key.toCharArray()) {
      result ^= (int) ch;
      result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
    }
    return result % servers.size();
  }
}

public class JavaClient {
  public static void main(String [] args) {
    Random random = new Random();
    if (args.length < 2) {
      System.out.println("Please provide a server and a port\n");
      return;
    }
    
    CacheClient client; 
    try {
      client = new CacheClient(args[0], Integer.parseInt(args[1]));
    } catch (Exception e) {
      System.out.println("Failed to create a client");
      return;
    }

    int count = 0;
    int numFailedPuts = 0;
    int numFailedGets = 0;

    while (true) {
     /* try {
        Thread.sleep(3);
      } catch (Exception e) {
      }*/
      count++;
      String key = Integer.toString(random.nextInt());
      if (random.nextBoolean()) {
        try {
          String value = Integer.toString(random.nextInt());
          client.put(key, value);
        } catch (Exception e) {
          numFailedPuts++;
          System.out.println("" + count);
          return;
        }
      } else {
        try {
          client.get(key);
        } catch (Exception e) {
          numFailedGets++;
          System.out.println("" + count);
          return;
        }
      }
    }
  }
}
