import java.util.List;

public class ConsistentHashing {
    public static int hash(String s) {
        int result = 0;
        for (int i = 0; i < s.length(); ++i) {
            result ^= s.charAt(i);
            result += (result << 1) + (result << 4) + (result << 7) + (result << 8) + (result << 24);
        }
        return result;
    }
    
    public static List<Server> getServer(int hash, View view) throws EmptyRingException {
        if (view.hashToServer.size() == 0) {
            throw new EmptyRingException("empty ring");
        }
        int minHash = 0, lb = 0;
        boolean find1 = false, find2 = false;
        for (int h: view.getHashToServer().keySet()) {
            if (!find1 || h < minHash) {
                find1 = true;
                minHash = h;
            }
            if (h < hash) {
                continue;
            }
            if (!find2 || h < lb) {
                find2 = true;
                lb = h;
            }
        }
        if (find2) {
            return view.getHashToServer().get(lb);
        }
        return view.getHashToServer().get(minHash);
    }
}
