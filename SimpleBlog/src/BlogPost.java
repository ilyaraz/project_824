import java.io.Serializable;

public class BlogPost implements Serializable {
    private String header;
    private String body;
    
    public BlogPost(String header, String body) {
        this.header = header;
        this.body = body;
    }
    
    public String toString() {
        return "<h2>"+header+"</h2><p>"+body+"</p>";
    }
}
