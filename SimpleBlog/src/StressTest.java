import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Random;
import java.util.Scanner;

public class StressTest {
    public static void main(String[] args) throws IOException {
        Random random = new Random();
        long t1 = System.currentTimeMillis();
        long counter = 0;
        for (;;) {
            int userID = random.nextInt(100000) + 1;
            URL url = new URL("http://palila.csail.mit.edu:8000/"+userID);
            HttpURLConnection connection = (HttpURLConnection)url.openConnection();
            connection.connect();
            long size = new Scanner(connection.getInputStream()).useDelimiter("\\A").next().length();
            connection.disconnect();
            counter++;
            if (counter % 1000 == 0) {
                long t2 = System.currentTimeMillis();
                System.out.println(counter * 1000 / (t2 - t1) + " " + size);
            }
        }
    }
}
