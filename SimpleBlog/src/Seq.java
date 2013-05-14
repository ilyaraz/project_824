import java.util.Arrays;

public class Seq {
    public char[] s;

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Seq seq = (Seq) o;

        if (!Arrays.equals(s, seq.s)) return false;

        return true;
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(s);
    }
}
