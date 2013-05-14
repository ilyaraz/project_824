import org.apache.thrift.TServiceClient;
import org.apache.thrift.TServiceClientFactory;
import org.apache.thrift.protocol.TBinaryProtocol;
import org.apache.thrift.protocol.TProtocol;
import org.apache.thrift.transport.TFramedTransport;
import org.apache.thrift.transport.TSocket;
import org.apache.thrift.transport.TTransport;
import org.apache.thrift.transport.TTransportException;

public class ServerConnection<T extends TServiceClient> {
    public ServerConnection(Server server, TServiceClientFactory<T> factory) throws TTransportException {
        TSocket socket = new TSocket(server.server, server.port);
        transport = new TFramedTransport(socket);
        TProtocol protocol = new TBinaryProtocol(transport);
        
        client = factory.getClient(protocol);
        transport.open();
    }
    
    public T getClient() {
        return client;
    }

    public void close() {
        transport.close();
    }

    private TTransport transport;
    private T client;
}
