import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.sql.*;
import java.util.ArrayList;
import java.util.Properties;
import java.util.Scanner;

public class HTTPServer implements HttpHandler {
    public static void main(String[] args) throws IOException, ViewServiceException {
        HttpServer server = HttpServer.create(new InetSocketAddress(8000), Integer.MAX_VALUE);
        server.createContext("/", new HTTPServer("header.txt", "footer.txt", new Server("128.30.48.203", 4057)));
        server.setExecutor(null);
        server.start();
    }
    
    public HTTPServer(String headerFile, String footerFile, Server viewServer) throws IOException, ViewServiceException {
        header = readFile(headerFile);
        footer = readFile(footerFile);
        client = new CacheClient(viewServer);
    }
    
    public static String readFile(String fileName) throws IOException {
        return new Scanner( new File(fileName) ).useDelimiter("\\A").next();
    }

    @Override
    public void handle(HttpExchange t) throws IOException {
        int userID;
        try {
            userID = Integer.parseInt(t.getRequestURI().toString().substring(1));
        }
        catch (Exception e) {
            userID = -1;
        }
        System.out.println("userID = " + userID);
        String response = header + processUser(userID) + footer;
        t.sendResponseHeaders(200, response.length());
        OutputStream os = t.getResponseBody();
        os.write(response.getBytes());
        os.close();
    }
    
    private String processUser(int userID) throws IOException {
        StringBuilder sb = new StringBuilder();
        sb.append("<div class = \"content\">");
        sb.append("<h1>User " + userID + "</h1>");
        ArrayList<BlogPost> posts = getPosts(userID);
        for (BlogPost post: posts) {
            sb.append(post);
        }
        sb.append("</div>");

        return sb.toString();
    }
    
    private ArrayList<BlogPost> getPosts(int userID) {
        try {
            System.out.println("querying cache");
            ArrayList<BlogPost> posts = (ArrayList<BlogPost>)client.get(Integer.toString(userID));
            // here one needs to check, if this data is fresh enough
            return posts;
        }
        catch (Exception e) {
            //e.printStackTrace();
            try {
                System.out.println("querying db");
                ArrayList<BlogPost> posts = getPostsFromDB(userID);
                try {
                    client.put(Integer.toString(userID), posts);
                }
                catch (Exception e1) {
                    //e1.printStackTrace();
                }
                return posts;
            }
            catch (SQLException e1) {
                //e1.printStackTrace();
                return new ArrayList<BlogPost>();
            }
        }
    }

    private ArrayList<BlogPost> getPostsFromDB(int userID) throws SQLException {
        Connection conn = null;
        Properties connectionProps = new Properties();
        connectionProps.put("user", "ilyaraz");
        connectionProps.put("password", "dbpass");


        conn = DriverManager.getConnection("jdbc:postgresql://towhee.csail.mit.edu:5432/ilyadb",
                connectionProps);
        Statement s = conn.createStatement();
        ResultSet r = s.executeQuery("SELECT * FROM posts WHERE uid='" + userID + "'");
        ArrayList<BlogPost> result = new ArrayList<BlogPost>();
        while (r.next()) {
            result.add(new BlogPost(r.getString(2), r.getString(3)));
        }
        conn.close();
        return result;
    }
    
    private String header;
    private String footer;
    private CacheClient client;
}
