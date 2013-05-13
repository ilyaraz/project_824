import java.io.*;

import sun.misc.BASE64Decoder;
import sun.misc.BASE64Encoder;

public class ObjectSerializer {
    public static String serialize(Serializable object) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(baos);
        oos.writeObject(object);
        oos.close();
        return new BASE64Encoder().encodeBuffer(baos.toByteArray());
    }

    public static Object deserialize(String data) throws IOException, ClassNotFoundException {
        ByteArrayInputStream bais = new ByteArrayInputStream(new BASE64Decoder().decodeBuffer(data));
        ObjectInputStream ois = new ObjectInputStream(bais);
        Object result;
        result = ois.readObject();
        ois.close();
        return result;
    }
}