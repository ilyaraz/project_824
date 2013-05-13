import java.io.IOException;
import java.io.Serializable;
import java.util.List;
import java.util.Random;

public class CacheClient {
    public CacheClient(Server viewServer) throws ViewServiceException {
        this.viewServer = viewServer;
        getView();
    }
    
    public void put(String key, Serializable value) throws Exception {
        ServerConnection<KVStorage.Client> conn = null;
        try {
            Server server = getServer(key);
            conn = new ServerConnection<KVStorage.Client>(server, new KVStorage.Client.Factory());
            PutArgs args = new PutArgs();
            args.key = key;
            args.value = ObjectSerializer.serialize(value);
            args.viewNum = reply.viewNum;
            PutReply reply = conn.getClient().put(args);
            if (reply.status == Status.WRONG_SERVER) {
                throw new WrongServerException("wrong server");
            }
        }
        catch (IOException e) {
            throw new InvalidValueException("can't serialize");
        }
        catch (Exception e) {
            getView();
            throw e;
        }
        finally {
            if (conn != null) {
                conn.close();
            }
        }
    }
    
    public Object get(String key) throws Exception {
        ServerConnection<KVStorage.Client> conn = null;
        try {
            Server server = getServer(key);
            conn = new ServerConnection<KVStorage.Client>(server, new KVStorage.Client.Factory());
            GetArgs args = new GetArgs();
            args.key = key;
            args.viewNum = reply.viewNum;
            GetReply reply = conn.getClient().get(args);
            if (reply.status == Status.WRONG_SERVER) {
                throw new WrongServerException("wrong server");
            }
            if (reply.status == Status.NO_KEY) {
                throw new NoKeyException("no key");
            }
            return ObjectSerializer.deserialize(reply.value);
        }
        catch (ClassNotFoundException e) {
            throw new InvalidValueException("failed to deserialize");
        }
        catch (NoKeyException e) {
            throw e;
        }
        catch (Exception e) {
            getView();
            throw e;
        }
        finally {
            if (conn != null) {
                conn.close();
            }
        }
    }

    private GetServersReply reply;
    private Server viewServer;
    private Random random = new Random();

    private void getView() throws ViewServiceException {
        ServerConnection<ViewService.Client> conn = null;
        try {
            conn = new ServerConnection<ViewService.Client>(viewServer, new ViewService.Client.Factory());
            reply = conn.getClient().getView();
        }
        catch (Exception e) {
            throw new ViewServiceException("can't connect to view service");
        }
        finally {
            if (conn != null) {
                conn.close();
            }
        }
    }
    
    private Server getServer(String key) throws EmptyRingException {
        int hash = ConsistentHashing.hash(key);
        List<Server> servers = ConsistentHashing.getServer(hash, reply.view);
        return servers.get(random.nextInt(servers.size()));
    }
}
