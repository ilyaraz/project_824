import java.io.File;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.sql.Time;
import java.util.Random;
import java.util.Scanner;

/**
 * Created by IntelliJ IDEA.
 * User: ilyaraz
 * Date: 5/12/13
 * Time: 4:25 PM
 * To change this template use File | Settings | File Templates.
 */
public class StressTest {
    public static void main(String[] args) throws MalformedURLException, IOException {
        Random random = new Random();
        for (;;) {
            int userID = random.nextInt();
            URL url = new URL("http://127.0.0.1:8000/"+userID);
            System.out.println(url);
            HttpURLConnection connection = (HttpURLConnection)url.openConnection();
            connection.connect();
            System.out.println(new Scanner(connection.getInputStream()).useDelimiter("\\A").next().length());
            connection.disconnect();
        }
    }
}
