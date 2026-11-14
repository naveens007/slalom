import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/**
 * Storer: writes the solution to the 'answer' file, using the C reference's
 * exact formats.
 */
public final class Storer {

    private Storer() {
    }

    /** @return false when the file could not be written. */
    static boolean store(Geometry geometry, Map<Channel, double[]> result) {
        try (PrintStream outfile = new PrintStream(new BufferedOutputStream(
                new FileOutputStream("answer")), false, StandardCharsets.UTF_8)) {
            outfile.print(String.format(CFormat.LOC, "%d patches:\n",
                    geometry.patchCount()));
            outfile.print(" Patch  Face       Position in w, h, d              Width     Height\n");
            for (Face face : geometry.faces()) {
                for (Patch patch : face) {
                    outfile.print(String.format(CFormat.LOC,
                            "%5d   %4d%11.5f%11.5f%11.5f  %11.5f%11.5f\n",
                            patch.index + 1, face.index + 1,
                            patch.place(0), patch.place(1), patch.place(2),
                            patch.width, patch.height));
                }
            }
            outfile.print("\n Patch  Face  Radiosities\n");
            for (Face face : geometry.faces()) {
                for (Patch patch : face) {
                    outfile.print(String.format(CFormat.LOC,
                            "%5d   %4d%12.8f%12.8f%12.8f\n",
                            patch.index + 1, face.index + 1,
                            result.get(Channel.R)[patch.index],
                            result.get(Channel.G)[patch.index],
                            result.get(Channel.B)[patch.index]));
                }
            }
        } catch (IOException e) {
            System.out.println("Unable to open 'answer' file.");
            return false;
        }
        return true;
    }
}
