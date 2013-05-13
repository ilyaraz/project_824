import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Properties;
import java.util.Random;

/**
 * Created by IntelliJ IDEA.
 * User: ilyaraz
 * Date: 5/12/13
 * Time: 5:44 PM
 * To change this template use File | Settings | File Templates.
 */
public class DBCreator {
    public static void main(String[] args) throws SQLException, IOException {
        Connection conn = null;
        Properties connectionProps = new Properties();
        connectionProps.put("user", "ilyaraz");
        connectionProps.put("password", "dbpass");


        conn = DriverManager.getConnection("jdbc:postgresql://towhee.csail.mit.edu:5432/ilyadb",
                connectionProps);
        System.out.println("Connected to database");
        Random random = new Random();
        PrintWriter out = new PrintWriter(new File("data.txt"));
        for (int i = 1; i <= 50000; i++) {
            System.out.println(i);
            for (int j = 0; j < 50; j++) {
                Statement stmt = conn.createStatement();
                String title = "";
                for (int k = 0; k < 10; k++) {
                    title += (char)(random.nextInt(26) + 'a');
                }
                String body = "";
                for (int k = 0; k < 1000; k++) {
                    body += (char)(random.nextInt(26) + 'a');
                    if (k % 10 == 0) {
                        body += ' ';
                    }
                }
                out.println(i + "\t'" + title + "'\t'" + body + "'");
                //stmt.executeUpdate("INSERT INTO posts VALUES (" + i + ", '" + title + "', '" + body + "')");
            }
        }
        out.close();
        conn.close();
    }
}
