import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Arrays;
import java.util.HashMap;
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
        String s = HTTPServer.readFile("pg4300.txt");
        s = s.replaceAll("\r", " ");
        s = s.replaceAll("\n", " ");
        final int PREFIX = 2;
        HashMap<Seq, HashMap<Character, Integer>> table = new HashMap<Seq, HashMap<Character, Integer>>();
        for (int i = 0; i + PREFIX < s.length(); i++) {
            Seq ss = new Seq();
            ss.s = new char[PREFIX];
            for (int j = i; j < i + PREFIX; j++) {
                ss.s[j - i] = s.charAt(j);
            }
            if (!table.containsKey(ss)) {
                table.put(ss, new HashMap<Character, Integer>());
            }
            if (!table.get(ss).containsKey(s.charAt(i + PREFIX))) {
                table.get(ss).put(s.charAt(i + PREFIX), 0);
            }
            table.get(ss).put(s.charAt(i + PREFIX), table.get(ss).get(s.charAt(i + PREFIX)) + 1);
        }
        Random random = new Random();
        PrintWriter out = new PrintWriter(new File("data.txt"));
        for (int i = 1; i <= 1000000; i++) {
            if (i % 1000 == 0) {
                System.out.println(i);
            }
            for (int j = 0; j < 10; j++) {
                String title = "";
                for (int k = 0; k < 10; k++) {
                    title += (char)(random.nextInt(26) + 'a');
                }
                String body = "";
                for (int k = 0; k < 300; k++) {
                    body += (char)(random.nextInt(26) + 'a');
                    if ((k + 1) % 5 == 0) {
                        body += ' ';
                    }
                }
                out.println(i + "\t" + title + "\t" + body);
            }
        }
        out.close();
    }
    
    private static String sampleCharacter(String cur, HashMap<Seq, HashMap<Character, Integer>> table, Random random) {
        char c = 'a';
        Seq ss = new Seq();
        ss.s = cur.toCharArray();
        int sum = 0;
        for (Character cc: table.get(ss).keySet()) {
            sum += table.get(ss).get(cc);
            if (random.nextInt(sum) < table.get(ss).get(cc)) {
                c = cc;
            }
        }
        return (cur + c).substring(1);
    }
}
