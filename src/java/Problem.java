import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The problem description: the box dimensions and the per-face optical
 * properties, read and validated from the 'geom' file (Reader in the C
 * reference).
 *
 * The 'geom' file is looked up next to the class files, with a fallback to
 * the parent directory so an in-tree `javac src/java/*.java` build finds
 * ../src/geom.
 */
public final class Problem {

    /** Box dimensions: x, y, z, the mirrored three, then x again. */
    private final double[] box = new double[7];

    /** Reflectivity of each of the six faces, per colour channel. */
    private final List<Spectrum> reflectivity = new ArrayList<Spectrum>();

    /** Emissivity of each of the six faces, per colour channel. */
    private final List<Spectrum> emissivity = new ArrayList<Spectrum>();

    private Problem() {
        for (int iface = 0; iface < 6; iface++) {
            reflectivity.add(new Spectrum());
            emissivity.add(new Spectrum());
        }
    }

    /** The box dimensions; only the first seven entries are meaningful. */
    public double[] box() {
        return box;
    }

    /** Reflectivity of face {@code iface}, per colour channel. */
    public Spectrum reflectivity(int iface) {
        return reflectivity.get(iface);
    }

    /** Emissivity of face {@code iface}, per colour channel. */
    public Spectrum emissivity(int iface) {
        return emissivity.get(iface);
    }

    /** Directory containing the class files, or null when undetectable. */
    private static Path classDirectory() {
        try {
            Path location = Paths.get(Problem.class.getProtectionDomain()
                    .getCodeSource().getLocation().toURI());
            if (Files.isRegularFile(location)) {
                /* Running from a jar: use the directory holding the jar. */
                Path parent = location.getParent();
                return parent != null ? parent : null;
            }
            return location;
        } catch (Exception | LinkageError e) {
            return null;
        }
    }

    /**
     * Locate the 'geom' file.  Like the C binary it lives next to the
     * executable (here: the class files); one level up is also accepted so
     * an in-tree build finds ../src/geom.
     */
    private static Path geomPath() {
        Path dir = classDirectory();
        if (dir != null) {
            Path nextToClasses = dir.resolve("geom");
            if (Files.isRegularFile(nextToClasses)) {
                return nextToClasses;
            }
            Path parentDir = dir.getParent();
            if (parentDir != null) {
                Path oneLevelUp = parentDir.resolve("geom");
                if (Files.isRegularFile(oneLevelUp)) {
                    return oneLevelUp;
                }
            }
        }
        return Paths.get("geom");
    }

    /**
     * Reads and validates the 'geom' file.
     *
     * @return the problem, or null after printing the C reference's error
     *         message
     */
    public static Problem read() {
        String text;
        try {
            text = new String(Files.readAllBytes(geomPath()), StandardCharsets.UTF_8);
        } catch (java.io.IOException e) {
            System.out.println("slalom:  'geom' geometry file not found.");
            return null;
        }

        /* The C code reads with fscanf() and discards the remainder of the
         * box line and of the third reflectivity/emissivity line.  Here the
         * numeric tokens are extracted from the whole file, which also
         * tolerates the trailing comments used by the shipped geom file. */
        Pattern number = Pattern.compile(
                "[-+]?(?:(?:\\d+\\.\\d*|\\.\\d+|\\d+)(?:[eE][-+]?\\d+)?)");
        Matcher matcher = number.matcher(text);
        double[] values = new double[39];
        int count = 0;
        while (count < 39 && matcher.find()) {
            values[count++] = Double.parseDouble(matcher.group());
        }
        if (count < 39) {
            System.out.println("slalom:  'geom' does not contain the required 39 numbers.");
            return null;
        }

        Problem problem = new Problem();
        problem.box[0] = values[0];
        problem.box[1] = values[1];
        problem.box[2] = values[2];

        /* Colour-major, exactly as the C reference reads them: one colour
         * channel after another, six faces per channel. */
        int idx = 3;
        for (Channel channel : Channel.values()) {
            for (int iface = 0; iface < 6; iface++) {
                problem.reflectivity.get(iface).set(channel, values[idx++]);
            }
        }
        for (Channel channel : Channel.values()) {
            for (int iface = 0; iface < 6; iface++) {
                problem.emissivity.get(iface).set(channel, values[idx++]);
            }
        }

        /* Sanity-check the values that were just read. */
        for (Channel channel : Channel.values()) {
            int j = channel.ordinal();
            if (problem.box[j] < 1.0 || problem.box[j] >= 100.0) {
                System.out.println("Box dimensions must be between 1 and 100.");
                return null;
            }
            problem.box[j + 3] = problem.box[j];

            double maxEmiss = 0.0;
            for (int iface = 0; iface < 6; iface++) {
                double rho = problem.reflectivity.get(iface).get(channel);
                double emiss = problem.emissivity.get(iface).get(channel);
                if (rho < 0.000 || rho > 0.999) {
                    System.out.println("Reflectivities must be between .000 and .999.");
                    return null;
                }
                if (emiss < 0.0) {
                    System.out.println("Emissivity cannot be negative.");
                    return null;
                }
                if (maxEmiss < emiss) {
                    maxEmiss = emiss;
                }
            }
            if (maxEmiss == 0.0) {
                System.out.println("Emissivities are zero.  Problem is trivial.");
                return null;
            }
        }
        problem.box[6] = problem.box[3];
        return problem;
    }
}
