import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.sql.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import java.util.Scanner;

public class Server implements HttpHandler {
    public static void main(String[] args) throws IOException {
        HttpServer server = HttpServer.create(new InetSocketAddress(8000), Integer.MAX_VALUE);
        server.createContext("/", new Server("header.txt", "footer.txt"));
        server.setExecutor(null);
        server.start();
    }
    
    public Server(String headerFile, String footerFile) throws IOException {
        header = readFile(headerFile);
        footer = readFile(footerFile);
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
    
    private String processUser(int userID) {
        StringBuilder sb = new StringBuilder();
        sb.append("<div class = \"content\">");
        sb.append("<h1>User " + userID + "</h1>");
        List<BlogPost> posts = getPosts(userID);
        for (BlogPost post: posts) {
            sb.append(post);
        }
        sb.append("</div>");
        return sb.toString();
    }
    
    private List<BlogPost> getPosts(int userID) {
        /*
        List<BlogPost> result = new ArrayList<BlogPost>();
        for (int i = 0; i < 10; i++) {
            result.add(new BlogPost("Header " + i, "Body " + i));
        }
        return result;
        */
        try {   
            Connection conn = null;
            Properties connectionProps = new Properties();
            connectionProps.put("user", "ilyaraz");
            connectionProps.put("password", "dbpass");
    
    
            conn = DriverManager.getConnection("jdbc:postgresql://towhee.csail.mit.edu:5432/ilyadb",
                    connectionProps);
            Statement s = conn.createStatement();
            ResultSet r = s.executeQuery("SELECT * FROM posts WHERE uid='" + userID + "'");
            List<BlogPost> result = new ArrayList<BlogPost>();
            while (r.next()) {
                result.add(new BlogPost(r.getString(2), r.getString(3)));
            }
            conn.close();
            return result;
        }
        catch (SQLException e) {
            e.printStackTrace();
            return new ArrayList<BlogPost>();
        }
    }
    
    private String header;
    private String footer;
}
