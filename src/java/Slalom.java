import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.math.MathContext;
import java.math.RoundingMode;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/******************************************************************************
 *                               S L A L O M
 *
 *     Scalable Language-independent Ames Laboratory One-minute Measurement
 *
 * Java port of the C reference implementation in ../slalom.c.  It follows the
 * reference closely: the same radiosity problem, the same seven timed phases
 * (Reader -> Region -> SetUp1/2/3 -> Solver -> Storer), the same analytic
 * FLOP counts, the same fixed-time bisection search for the problem size n,
 * and the same console and 'answer' file output formats.
 *
 * Command line (identical to the C driver):
 *
 *     java Slalom -t <seconds> -l <integer> -u <integer>
 *     java Slalom --time <seconds> --lower-bound integer --upper-bound integer
 *
 * The 'geom' file is looked up next to the class files (with a fallback to
 * the parent directory, so an in-tree `javac src/java/Slalom.java` build finds
 * ../src/geom).  The 'answer' file is written to the current working
 * directory, matching the C convention.
 *
 * John Gustafson, Diane Rover, Michael Carter, and Stephen Elbert
 * Ames Laboratory, 3/18/90 (reference C implementation)
 *****************************************************************************/
public final class Slalom {

    /* NMAX = Largest npatch for this computer; overridable for experiments. */
    private static final int NMAX = Integer.getInteger("SLALOM_NMAX", 8192);
    private static final double EPS = 0.5e-8;

    private static final String[] TASKS = {
        "Reader", "Region", "SetUp1", "SetUp2", "SetUp3", "Solver", "Storer"};

    private static final Locale LOC = Locale.US;

    private Slalom() {
    }

    /* ------------------------------------------------------------------ */
    /*  C-compatible %g formatting.                                        */
    /*                                                                     */
    /*  java.util.Formatter's %g keeps trailing zeros ("0.500000"), while   */
    /*  C printf %g strips them ("0.5") and switches to exponent form at    */
    /*  different thresholds.  The benchmark output is defined by the C     */
    /*  reference, so %g is re-implemented here to print identical text.    */
    /* ------------------------------------------------------------------ */

    /** Format like C's "%.{precision}g". */
    static String g(double v, int precision) {
        if (v == 0.0) {
            return "0";
        }
        if (Double.isNaN(v)) {
            return "NaN";
        }
        if (Double.isInfinite(v)) {
            return v > 0.0 ? "Inf" : "-Inf";
        }
        int p = (precision <= 0) ? 1 : precision;

        /* Round to p significant digits (C uses round-to-nearest). */
        String sci = String.format(LOC, "%." + (p - 1) + "e", v);
        int expVal = Integer.parseInt(sci.substring(sci.indexOf('e') + 1));

        String s;
        if (expVal < -4 || expVal >= p) {
            /* Scientific notation: strip trailing zeros from the mantissa,
             * exponent with at least two digits. */
            String mant = sci.substring(0, sci.indexOf('e'));
            if (mant.indexOf('.') >= 0) {
                mant = mant.replaceAll("0+$", "");
                if (mant.endsWith(".")) {
                    mant = mant.substring(0, mant.length() - 1);
                }
            }
            String ePart = sci.substring(sci.indexOf('e') + 1);
            boolean negativeExponent = ePart.startsWith("-");
            if (ePart.startsWith("+") || ePart.startsWith("-")) {
                ePart = ePart.substring(1);
            }
            while (ePart.length() < 2) {
                ePart = "0" + ePart;
            }
            s = mant + "e" + (negativeExponent ? "-" : "+") + ePart;
        } else {
            /* Decimal notation: p significant digits, trailing zeros gone. */
            String fixed = new java.math.BigDecimal(v)
                    .round(new MathContext(p, RoundingMode.HALF_UP))
                    .toPlainString();
            if (fixed.indexOf('.') >= 0) {
                fixed = fixed.replaceAll("0+$", "");
                if (fixed.endsWith(".")) {
                    fixed = fixed.substring(0, fixed.length() - 1);
                }
            }
            s = fixed;
        }
        return s;
    }

    /** Format like C's "%g" (default precision 6). */
    static String g(double v) {
        return g(v, 6);
    }

    /* ------------------------------------------------------------------ */
    /*  Wall clock in seconds (like gettimeofday in the C reference).       */
    /* ------------------------------------------------------------------ */

    private static double when() {
        return System.nanoTime() * 1e-9;
    }

    /* ------------------------------------------------------------------ */
    /*  Reader: reads the problem description from the 'geom' file.         */
    /* ------------------------------------------------------------------ */

    /** Directory containing the class files, or null when undetectable. */
    private static Path classDirectory() {
        try {
            Path location = Paths.get(Slalom.class.getProtectionDomain()
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
     * an in-tree `javac src/java/Slalom.java` finds ../src/geom.
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

    /** Returns false on failure; box/rho/emiss are filled on success. */
    private static boolean reader(double[] box, double[][] rho, double[][] emiss) {
        Path path = geomPath();
        String text;
        try {
            text = new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
        } catch (IOException e) {
            System.out.println("slalom:  'geom' geometry file not found.");
            return false;
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
            return false;
        }

        box[0] = values[0];
        box[1] = values[1];
        box[2] = values[2];
        int idx = 3;
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 6; i++) {
                rho[i][j] = values[idx++];
            }
        }
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 6; i++) {
                emiss[i][j] = values[idx++];
            }
        }

        /* Sanity-check the values that were just read. */
        for (int j = 0; j < 3; j++) {
            if (box[j] < 1.0 || box[j] >= 100.0) {
                System.out.println("Box dimensions must be between 1 and 100.");
                return false;
            }
            box[j + 3] = box[j];

            double maxEmiss = 0.0;
            for (int i = 0; i < 6; i++) {
                if (rho[i][j] < 0.000 || rho[i][j] > 0.999) {
                    System.out.println("Reflectivities must be between .000 and .999.");
                    return false;
                }
                if (emiss[i][j] < 0.0) {
                    System.out.println("Emissivity cannot be negative.");
                    return false;
                }
                if (maxEmiss < emiss[i][j]) {
                    maxEmiss = emiss[i][j];
                }
            }
            if (maxEmiss == 0.0) {
                System.out.println("Emissivities are zero.  Problem is trivial.");
                return false;
            }
        }
        box[6] = box[3];
        return true;
    }

    /* ------------------------------------------------------------------ */
    /*  Region: decomposes the surface of the box into patches that are     */
    /*  as nearly equal in size and square as possible.                     */
    /* ------------------------------------------------------------------ */

    /** Result of Region(); null means the decomposition failed. */
    private static final class Region {
        final int[][] loop;
        final double[][] place;
        final double[][] size;
        final double[] area;

        Region(int[][] loop, double[][] place, double[][] size, double[] area) {
            this.loop = loop;
            this.place = place;
            this.size = size;
            this.area = area;
        }
    }

    private static Region region(int npatch, double[] box) {
        int[][] loop = new int[6][2];
        double[][] place = new double[3][npatch];
        double[][] size = new double[2][npatch];
        double[] area = new double[npatch];

        /* Allocate patches to each face, proportionate to area of each face. */
        double tmp1 = 2.0 * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
        double tmp2 = 0.0;
        double tmp3 = npatch;
        loop[0][0] = 0;
        for (int iface = 0; iface < 5; iface++) {
            tmp2 = tmp2 + box[iface] * box[iface + 1];
            loop[iface][1] = (int) (tmp3 * tmp2 / tmp1 + 0.5) - 1;
            loop[iface + 1][0] = loop[iface][1] + 1;
        }
        loop[5][1] = npatch - 1;

        /* Subdivide each face into numpat patches. */
        for (int iface = 0; iface < 6; iface++) {
            int numpat = loop[iface][1] - loop[iface][0] + 1;
            tmp3 = 0.0;
            if (iface >= 3) {
                tmp3 = box[iface - 1];
            }
            int numcol = (int) (Math.sqrt(numpat * box[iface] / box[iface + 1]) + 0.5);
            if (numcol > numpat) {
                numcol = numpat;
            }
            if (numcol == 0) {
                numcol = 1;
            }
            double width = box[iface] / numcol;
            int itmp1 = numcol - 1;
            tmp1 = 0.0;
            for (int icol = 0; icol < numcol; icol++) {
                int itmp2 = itmp1 / numcol;
                int numrow = (itmp1 + numpat) / numcol - itmp2;
                if (numrow == 0) {
                    System.out.println("Eccentric box requires more patches.");
                    return null;
                }
                double height = box[iface + 1] / numrow;
                tmp2 = 0.0;
                double tmp4 = width * height * (8.0 * Math.PI);
                int lead = loop[iface][0] + itmp2;
                int last = lead + numrow;
                for (int ipatch = lead; ipatch < last; ipatch++) {
                    size[0][ipatch] = width;
                    size[1][ipatch] = height;
                    place[0][ipatch] = tmp1;
                    place[1][ipatch] = tmp2;
                    place[2][ipatch] = tmp3;
                    area[ipatch] = tmp4;
                    tmp2 = tmp2 + height;
                }
                tmp1 = tmp1 + width;
                itmp1 = itmp1 + numpat;
            }
        }
        return new Region(loop, place, size, area);
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp1: form factors for parallel patches.                          */
    /* ------------------------------------------------------------------ */

    private static void setUp1(int npatch, int[][] loop, double[][] coeff,
            double[][] place, double[][] size) {
        double[][][] d = new double[2][2][2];
        double[][][] d2 = new double[2][2][2];

        for (int iface = 0; iface < 3; iface++) {
            int jface = iface + 3;
            double tmp1 = place[2][loop[jface][0]] * place[2][loop[jface][0]];
            for (int ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                for (int jpatch = loop[jface][0]; jpatch <= loop[jface][1]; jpatch++) {
                    for (int j = 0; j < 2; j++) {
                        d[0][0][j] = place[j][jpatch] - place[j][ipatch];
                        d[1][0][j] = d[0][0][j] + size[j][jpatch];
                        d[0][1][j] = d[0][0][j] - size[j][ipatch];
                        d[1][1][j] = d[1][0][j] - size[j][ipatch];
                        d2[0][0][j] = d[0][0][j] * d[0][0][j];
                        d2[1][0][j] = d[1][0][j] * d[1][0][j];
                        d2[0][1][j] = d[0][1][j] * d[0][1][j];
                        d2[1][1][j] = d[1][1][j] * d[1][1][j];
                    }
                    double tmp2 = 0.0;
                    for (int j = 0; j < 2; j++) {
                        for (int m = 0; m < 2; m++) {
                            for (int i = 0; i < 2; i++) {
                                double tmp3 = d2[m][i][1 - j] + tmp1;
                                double tmp4 = Math.sqrt(tmp3);
                                double tmp5 = 1.0 / tmp4;
                                double tmp8 = 0.0;
                                for (int k = 0; k < 2; k++) {
                                    for (int n = 0; n < 2; n++) {
                                        double tmp6 = d2[k][n][j];
                                        double tmp7 = d[k][n][j];
                                        tmp2 = (tmp3 - tmp6) * Math.log(tmp6 + tmp3) - tmp2;
                                        tmp8 = -tmp7 * Math.atan(tmp7 * tmp5) - tmp8;
                                    }
                                    tmp2 = -tmp2;
                                    tmp8 = -tmp8;
                                }
                                tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                            }
                            tmp2 = -tmp2;
                        }
                    }
                    coeff[ipatch][jpatch] = tmp2;
                    coeff[jpatch][ipatch] = tmp2;
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp2: form factors for orthogonal patches.                        */
    /* ------------------------------------------------------------------ */

    private static void setUp2(int npatch, int[][] loop, double[][] coeff,
            double[][] place, double[][] size) {
        double[][] r = new double[2][2];
        double[][] r2 = new double[2][2];
        double[][] s = new double[2][2];

        for (int iface = 0; iface < 6; iface++) {
            for (int m = 0; m < 2; m++) {
                int jface = (iface + m + 1) % 6;
                for (int ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                    for (int jpatch = loop[jface][0]; jpatch <= loop[jface][1]; jpatch++) {
                        r[0][0] = place[m][jpatch] - place[1 - m][ipatch];
                        r[1][0] = r[0][0] + size[m][jpatch];
                        r[0][1] = r[0][0] - size[1 - m][ipatch];
                        r[1][1] = r[1][0] - size[1 - m][ipatch];
                        s[0][0] = place[m][ipatch] - place[2][jpatch];
                        s[1][0] = s[0][0] + size[m][ipatch];
                        s[0][1] = place[1 - m][jpatch] - place[2][ipatch];
                        s[1][1] = s[0][1] + size[1 - m][jpatch];
                        for (int i = 0; i < 2; i++) {
                            for (int j = 0; j < 2; j++) {
                                s[i][j] = s[i][j] * s[i][j];
                                r2[i][j] = r[i][j] * r[i][j];
                            }
                            /* Bump the term by a small real to avoid
                             * singularities in the coupling function: */
                            s[i][1] = s[i][1] + 1e-35;
                        }
                        double tmp2 = 0.0;
                        for (int i = 0; i < 2; i++) {
                            for (int j = 0; j < 2; j++) {
                                double tmp3 = s[i][0] + s[j][1];
                                double tmp4 = Math.sqrt(tmp3);
                                double tmp5 = 1.0 / tmp4;
                                double tmp8 = 0.0;
                                for (int k = 0; k < 2; k++) {
                                    for (int n = 0; n < 2; n++) {
                                        double tmp6 = r2[k][n];
                                        double tmp7 = r[k][n];
                                        tmp2 = (tmp3 - tmp6) * Math.log(tmp6 + tmp3) - tmp2;
                                        tmp8 = -tmp7 * Math.atan(tmp7 * tmp5) - tmp8;
                                    }
                                    tmp2 = -tmp2;
                                    tmp8 = -tmp8;
                                }
                                tmp2 = -4.0 * tmp4 * tmp8 - tmp2;
                            }
                            tmp2 = -tmp2;
                        }
                        tmp2 = Math.abs(tmp2);
                        coeff[ipatch][jpatch] = tmp2;
                        coeff[jpatch][ipatch] = tmp2;
                    }
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  SetUp3: normalise row sums to 1 and fold in rho and emissivities.   */
    /* ------------------------------------------------------------------ */

    private static boolean setUp3(int npatch, int[][] loop, double[] area,
            double[][] rho, double[][] emiss, double[][] coeff,
            double[][] diag, double[][] rhs) {
        double[] vtmp1 = new double[3];
        double[] vtmp2 = new double[3];

        for (int iface = 0; iface < 6; iface++) {
            for (int j = 0; j < 3; j++) {
                vtmp1[j] = 1.0 / rho[iface][j];
                vtmp2[j] = emiss[iface][j] * vtmp1[j];
            }
            for (int ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                double tmp1 = 0.0;
                for (int jpatch = 0; jpatch < loop[iface][0]; jpatch++) {
                    tmp1 += coeff[ipatch][jpatch];
                }
                for (int jpatch = loop[iface][1] + 1; jpatch < npatch; jpatch++) {
                    tmp1 += coeff[ipatch][jpatch];
                }
                /* Make sure the row sum (total form factor) is close to 1: */
                if (Math.abs(tmp1 - area[ipatch]) > (0.5e-9 * tmp1)) {
                    System.out.println("Total form factor is too far from unity.");
                    return false;
                }
                tmp1 = -tmp1;
                /* Set coplanar patch interactions to zero. */
                for (int jpatch = loop[iface][0]; jpatch <= loop[iface][1]; jpatch++) {
                    coeff[ipatch][jpatch] = 0.0;
                }
                /* Assign diagonal entries and right-hand sides. */
                for (int j = 0; j < 3; j++) {
                    diag[j][ipatch] = vtmp1[j] * tmp1;
                    rhs[j][ipatch] = vtmp2[j] * tmp1;
                }
            }
        }
        return true;
    }

    /* ------------------------------------------------------------------ */
    /*  Solver: LDL^T factorisation and backsolve, per colour channel.      */
    /*  No pivoting; the matrix is diagonally dominant.                     */
    /* ------------------------------------------------------------------ */

    /** Dot product of the first n elements of two vectors (unit stride). */
    private static double ddot(double[] a, double[] b, int n) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += a[i] * b[i];
        }
        return sum;
    }

    private static void solver(int npatch, int non0, double[][] coeff,
            double[][] diag, double[][] rhs, double[][] result) {
        for (int m = 0; m < 3; m++) {
            /* Load the lower triangle of coefficients, diagonal, and
             * solution vector. */
            for (int i = non0; i < npatch; i++) {
                coeff[i][i] = diag[m][i];
                result[m][i] = rhs[m][i];
                for (int j = 0; j < i; j++) {
                    coeff[i][j] = coeff[j][i];
                }
            }

            /* Factor the matrix, writing factors over the original matrix. */
            for (int j = 0; j < non0; j++) {
                coeff[j][j] = 1.0 / diag[m][j];
                result[m][j] = rhs[m][j];
            }

            for (int j = non0; j < npatch; j++) {
                for (int k = non0; k < j; k++) {
                    coeff[j][k] -= ddot(coeff[k], coeff[j], k);
                }
                for (int k = 0; k < j; k++) {
                    double tmp1 = coeff[j][k];
                    coeff[j][k] = tmp1 * coeff[k][k];
                    coeff[j][j] -= tmp1 * coeff[j][k];
                }
                coeff[j][j] = 1.0 / coeff[j][j];
            }

            /* Backsolve, in three stages (for L, D, and L transpose). */
            for (int k = non0; k < npatch; k++) {
                result[m][k] -= ddot(result[m], coeff[k], k);
            }

            for (int k = 0; k < npatch; k++) {
                result[m][k] *= coeff[k][k];
            }

            /* Column access across rows (the C code uses stride NMAX): */
            for (int k = npatch - 2; k >= non0; k--) {
                double sum = 0.0;
                for (int j = k + 1; j < npatch; j++) {
                    sum += result[m][j] * coeff[j][k];
                }
                result[m][k] -= sum;
            }

            for (int k = non0 - 1; k >= 0; k--) {
                double sum = 0.0;
                for (int j = non0; j < npatch; j++) {
                    sum += result[m][j] * coeff[j][k];
                }
                result[m][k] -= sum;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Storer: writes the answer to the 'answer' file.                     */
    /* ------------------------------------------------------------------ */

    private static boolean storer(int npatch, int[][] loop, double[][] result,
            double[][] place, double[][] size) {
        try (PrintStream outfile = new PrintStream(new BufferedOutputStream(
                new FileOutputStream("answer")), false, StandardCharsets.UTF_8)) {
            outfile.print(String.format(LOC, "%d patches:\n", npatch));
            outfile.print(" Patch  Face       Position in w, h, d              Width     Height\n");
            for (int iface = 0; iface < 6; iface++) {
                for (int ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                    outfile.print(String.format(LOC,
                            "%5d   %4d%11.5f%11.5f%11.5f  %11.5f%11.5f\n",
                            ipatch + 1, iface + 1,
                            place[0][ipatch], place[1][ipatch], place[2][ipatch],
                            size[0][ipatch], size[1][ipatch]));
                }
            }
            outfile.print("\n Patch  Face  Radiosities\n");
            for (int iface = 0; iface < 6; iface++) {
                for (int ipatch = loop[iface][0]; ipatch <= loop[iface][1]; ipatch++) {
                    outfile.print(String.format(LOC,
                            "%5d   %4d%12.8f%12.8f%12.8f\n",
                            ipatch + 1, iface + 1,
                            result[0][ipatch], result[1][ipatch], result[2][ipatch]));
                }
            }
        } catch (IOException e) {
            System.out.println("Unable to open 'answer' file.");
            return false;
        }
        return true;
    }

    /* ------------------------------------------------------------------ */
    /*  Verify: checks that the computed radiosities satisfy the equations. */
    /*  Runs after the clock has stopped.                                   */
    /* ------------------------------------------------------------------ */

    private static boolean verify(int npatch, double[][] coeff, double[][] diag,
            double[][] rhs, double[][] result) {
        double tmp1 = 0.0;
        double anorm = 0.0;
        double xnorm = 0.0;

        for (int m = 0; m < 3; m++) {
            /* Copy the lower triangle of the coefficients to the upper
             * triangle, and load the diagonal. */
            for (int i = 0; i < npatch; i++) {
                coeff[i][i] = diag[m][i];
                for (int j = 0; j < i; j++) {
                    coeff[i][j] = coeff[j][i];
                }
            }
            /* Multiply the matrix by the solution vector, and accumulate
             * the norm of the residual. */
            anorm = 0.0;
            xnorm = 0.0;
            for (int j = 0; j < npatch; j++) {
                double tmp2 = rhs[m][j];
                for (int i = 0; i < npatch; i++) {
                    tmp2 -= coeff[j][i] * result[m][i];
                    anorm = Math.max(anorm, Math.abs(coeff[j][i]));
                }
                xnorm = Math.max(xnorm, Math.abs(result[m][j]));
                tmp1 += Math.abs(tmp2);
            }
        }
        tmp1 /= (anorm * xnorm);
        if (tmp1 > 3.0 * EPS) {
            System.out.println("Residual is too large: " + g(tmp1));
            return false;
        }
        return true;
    }

    /* ------------------------------------------------------------------ */
    /*  Meter: times one complete run of the benchmark at size npatch.      */
    /* ------------------------------------------------------------------ */

    /** Result of Meter(); ok is false when the run was rejected. */
    private static final class MeterResult {
        final boolean ok;
        final double timing;
        final double work;

        MeterResult(boolean ok, double timing, double work) {
            this.ok = ok;
            this.timing = timing;
            this.work = work;
        }
    }

    private static MeterResult meter(int npatch) {
        /* First check that npatch lies between 6 and NMAX: */
        if (npatch < 6) {
            System.out.println("Must be at least 6, the number of faces.");
            return new MeterResult(false, 0.0, 0.0);
        }
        if (npatch > NMAX) {
            System.out.printf(LOC, "Exceeds %d = maximum for this system.\n", NMAX);
            return new MeterResult(false, 0.0, 0.0);
        }

        /* Ensure that a previous 'answer' file is deleted: */
        try {
            Files.deleteIfExists(Paths.get("answer"));
        } catch (IOException e) {
            /* Ignore; Storer overwrites it anyway. */
        }

        double[] box = new double[7];
        double[][] rho = new double[6][3];
        double[][] emiss = new double[6][3];
        double[][] coeff = new double[npatch][npatch];
        double[][] diag = new double[3][npatch];
        double[][] place = new double[3][npatch];
        double[][] result = new double[3][npatch];
        double[][] rhs = new double[3][npatch];
        double[][] size = new double[2][npatch];

        /* Time the tasks, individually and collectively. */
        double[] sec = new double[8];
        sec[0] = when();
        if (!reader(box, rho, emiss)) {
            return new MeterResult(false, 0.0, 0.0);
        }
        sec[1] = when();

        Region region = region(npatch, box);
        if (region == null) {
            return new MeterResult(false, 0.0, 0.0);
        }
        int[][] loop = region.loop;
        double[][] placeArr = region.place;
        double[][] sizeArr = region.size;
        double[] area = region.area;
        sec[2] = when();

        setUp1(npatch, loop, coeff, placeArr, sizeArr);
        sec[3] = when();
        setUp2(npatch, loop, coeff, placeArr, sizeArr);
        sec[4] = when();
        if (!setUp3(npatch, loop, area, rho, emiss, coeff, diag, rhs)) {
            return new MeterResult(false, 0.0, 0.0);
        }
        sec[5] = when();
        int non0 = loop[1][0];
        solver(npatch, non0, coeff, diag, rhs, result);
        sec[6] = when();
        if (!storer(npatch, loop, result, placeArr, sizeArr)) {
            return new MeterResult(false, 0.0, 0.0);
        }
        sec[7] = when();
        double timing = sec[7] - sec[0];
        double[] secs = new double[7];
        for (int i = 0; i < 7; i++) {
            secs[i] = sec[i + 1] - sec[i];
        }

        /* Assess the floating-point work done by each routine, and total.
         * The counts are tallied into doubles to prevent integer overflow. */
        double[] p = new double[6];
        int itmp1 = 0;
        double tmp1 = 0.0;
        for (int i = 0; i < 6; i++) {
            p[i] = loop[i][1] - loop[i][0] + 1;
            tmp1 += p[i] * p[i];
            /* int addition truncates, exactly like the C reference. */
            itmp1 += Math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5;
        }
        double tmp2 = p[0] * p[3] + p[1] * p[4] + p[2] * p[5];
        double[] ops = new double[7];
        ops[0] = 258.0;
        ops[1] = 154.0 + 8.0 * itmp1 + npatch;
        ops[2] = 3.0 + 840.0 * tmp2;
        ops[3] = 430.0 * ((double) npatch * npatch - tmp1 - 2.0 * tmp2) / 2.0;
        ops[4] = 72.0 + 9.0 * npatch + (double) npatch * npatch - tmp1;
        ops[5] = npatch * (npatch * ((double) npatch + 7.5) - 2.5) - 21.0
                + (non0 + 1.0) * ((non0 + 1.0) * (2.0 * ((double) non0 + 1.0) - 16.5) + 35.5)
                + (non0 + 1.0) * npatch * (9.0 - 3.0 * ((double) non0 + 1.0));
        ops[6] = 48.0 * npatch;
        double work = ops[0] + ops[1] + ops[2] + ops[3] + ops[4] + ops[5] + ops[6];

        /* Display the timing-work-speed breakdown by routine. */
        System.out.printf(LOC, "%d patches:\n", npatch);
        System.out.print(" Task  Seconds       Operations        MFLOPS    % of Time\n");
        String format = "%6.6s%8.3f%17.0f%14.6f%10.1f %%\n";
        for (int i = 0; i < 7; i++) {
            if (secs[i] == 0.0) {
                secs[i] = 0.001;
            }
            System.out.printf(LOC, format, TASKS[i], secs[i], ops[i],
                    (ops[i] / secs[i]) * 1e-6, 100.0 * secs[i] / timing);
        }
        System.out.printf(LOC, format, "TOTALS", timing, work,
                (work / timing) * 1e-6, 100.0);
        verify(npatch, coeff, diag, rhs, result);

        return new MeterResult(true, timing, work);
    }

    /* ------------------------------------------------------------------ */
    /*  Machine information (the Java counterpart of system_info.c).        */
    /* ------------------------------------------------------------------ */

    /** Reads the value of a "key: value" line from a text file, or null. */
    private static String readFirstField(String path, String key) {
        try (BufferedReader in = Files.newBufferedReader(Paths.get(path),
                StandardCharsets.UTF_8)) {
            String line;
            while ((line = in.readLine()) != null) {
                if (line.startsWith(key)) {
                    int colon = line.indexOf(':');
                    if (colon >= 0) {
                        String value = line.substring(colon + 1).trim();
                        if (!value.isEmpty()) {
                            return value;
                        }
                    }
                }
            }
        } catch (IOException e) {
            /* Fall through. */
        }
        return null;
    }

    /** Reads a whole (usually one line) text file, trimmed, or null. */
    private static String readTextFile(String path) {
        try {
            String text = new String(Files.readAllBytes(Paths.get(path)),
                    StandardCharsets.UTF_8);
            /* The device tree strings are NUL padded. */
            return text.replace("\u0000", "").trim();
        } catch (IOException e) {
            return null;
        }
    }

    /** Best-effort human readable processor name. */
    private static String detectProcessorName() {
        String name = readTextFile("/sys/firmware/devicetree/base/model");
        if (name == null) {
            name = readFirstField("/proc/cpuinfo", "Hardware");
        }
        if (name == null) {
            name = readFirstField("/proc/cpuinfo", "model name");
        }
        if (name == null) {
            name = readFirstField("/proc/cpuinfo", "cpu model");
        }
        if (name == null) {
            name = System.getProperty("os.arch", "unknown");
        }
        return name;
    }

    /** Best-effort CPU frequency in GHz; 0.0 when unavailable. */
    private static double detectClockSpeed() {
        String[] freqFiles = {
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
            "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"};
        for (String path : freqFiles) {
            String value = readTextFile(path);
            if (value != null) {
                try {
                    double khz = Double.parseDouble(value.split("\\s+")[0]);
                    if (khz > 0.0) {
                        return khz / 1.0e6; /* kHz -> GHz */
                    }
                } catch (NumberFormatException e) {
                    /* Try the next source. */
                }
            }
        }
        String mhz = readFirstField("/proc/cpuinfo", "cpu MHz");
        if (mhz != null) {
            try {
                double value = Double.parseDouble(mhz);
                if (value > 0.0) {
                    return value / 1.0e3; /* MHz -> GHz */
                }
            } catch (NumberFormatException e) {
                /* Fall through. */
            }
        }
        return 0.0;
    }

    /** Parses a sysfs cache size like "32K" or "1M" into KiB. */
    private static int cacheKib(String value) {
        if (value == null || value.isEmpty()) {
            return 0;
        }
        value = value.trim();
        double multiplier = 1.0;
        if (value.endsWith("K") || value.endsWith("k")) {
            value = value.substring(0, value.length() - 1);
        } else if (value.endsWith("M") || value.endsWith("m")) {
            value = value.substring(0, value.length() - 1);
            multiplier = 1024.0;
        }
        try {
            return (int) (Double.parseDouble(value) * multiplier);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    /** {L1i, L2} cache sizes in KiB, from sysfs when possible. */
    private static int[] detectCaches() {
        int l1i = 0;
        int l2 = 0;
        Path base = Paths.get("/sys/devices/system/cpu/cpu0/cache");
        try (DirectoryStream<Path> entries = Files.newDirectoryStream(base)) {
            for (Path entry : entries) {
                String level = readTextFile(entry.resolve("level").toString());
                String kind = readTextFile(entry.resolve("type").toString());
                int size = cacheKib(readTextFile(entry.resolve("size").toString()));
                if ("1".equals(level) && kind != null
                        && kind.toLowerCase(LOC).startsWith("instruction")) {
                    l1i = size;
                } else if ("2".equals(level)) {
                    l2 = size;
                }
            }
        } catch (IOException e) {
            /* No sysfs cache data. */
        }
        return new int[] {l1i, l2};
    }

    /** Returns {total GiB, used GiB} from /proc/meminfo, or {0, 0}. */
    private static double[] memoryGib() {
        double total = 0.0;
        double free = 0.0;
        String memTotal = readFirstField("/proc/meminfo", "MemTotal");
        String memFree = readFirstField("/proc/meminfo", "MemFree");
        if (memTotal != null) {
            try {
                total = Double.parseDouble(memTotal.split("\\s+")[0]) / (1024.0 * 1024.0);
            } catch (NumberFormatException e) {
                total = 0.0;
            }
        }
        if (memFree != null) {
            try {
                free = Double.parseDouble(memFree.split("\\s+")[0]) / (1024.0 * 1024.0);
            } catch (NumberFormatException e) {
                free = 0.0;
            }
        }
        return new double[] {total, total - free};
    }

    private static String hostName() {
        try {
            return InetAddress.getLocalHost().getHostName();
        } catch (IOException | RuntimeException e) {
            String fromEnv = System.getenv("COMPUTERNAME");
            return (fromEnv != null) ? fromEnv : "unknown";
        }
    }

    /** Prints the machine report, mirroring system_info()'s layout. */
    private static void systemInfo() {
        String arch = System.getProperty("os.arch", "unknown");
        String os = System.getProperty("os.name", "unknown");
        String proc = detectProcessorName();
        double clock = detectClockSpeed();
        int[] caches = detectCaches();
        double[] mem = memoryGib();
        double memUsage = (mem[0] > 0.0) ? (mem[1] / mem[0]) * 100.0 : 0.0;
        int cores = Runtime.getRuntime().availableProcessors();
        String user = System.getProperty("user.name", "unknown");
        String node = hostName();
        String version = System.getProperty("java.version", "unknown");
        LocalDateTime now = LocalDateTime.now();
        String date = String.format(LOC, "%02d/%02d/%04d (dd/mm) %02d:%02d:%02d",
                now.getDayOfMonth(), now.getMonthValue(), now.getYear(),
                now.getHour(), now.getMinute(), now.getSecond());

        System.out.printf(LOC, "Machine:%s\t\t Processor:  %s\n", arch, proc);
        System.out.printf(LOC, "Memory: %3.2f%% \t\t # of procs: %d\n", memUsage, cores);
        System.out.printf(LOC, "L1 iCache:%d KB\t\t # used:1\n", caches[0]);
        if (clock > 0.0) {
            System.out.printf(LOC, "NMAX:%5d \t\t Clock:%5.2fGHz\n", NMAX, clock);
        } else {
            /* The frequency cannot always be determined; do not pretend
             * it is 0.00GHz. */
            System.out.printf(LOC, "NMAX:%5d \t\t Clock:   N/A\n", NMAX);
        }
        System.out.printf(LOC, "OS:%s\t\t Timer:Wall, System.nanoTime()\n", os);
        System.out.printf(LOC, "L2 Cache:%d KB\t\t Node: %s\n", caches[1], node);
        System.out.printf(LOC, "Language:Java\t\t Version:%s\n", version);
        System.out.printf(LOC, "Compiler:javac\t\t Run by:%s\n", user);
        System.out.printf(LOC, "Options:standard Java\t Date:%s\n", date);
    }

    /* ------------------------------------------------------------------ */
    /*  What: prints the machine report and the final statistics.          */
    /* ------------------------------------------------------------------ */

    private static void what(int n, double timing, double work) {
        System.out.print("\n");
        systemInfo();

        System.out.print("\n");
        System.out.printf(LOC, "M ops:    %-13s        Time:       %.3f seconds\n",
                g(work * 1e-6), timing);
        System.out.printf(LOC, "n:        %-6d               MFLOPS:     %s\n",
                n, g((work / timing) * 1e-6, 5));
        System.out.printf(LOC, "Approximate data memory use: %d bytes.\n",
                8L * n * n + 120L * n + 800);
    }

    /* ------------------------------------------------------------------ */
    /*  Driver (the Java counterpart of sl-driver.c).                       */
    /* ------------------------------------------------------------------ */

    private static final class Params {
        double goal;
        int lBound;
        int uBound;
    }

    private static int errmsg(String msg) {
        System.err.println("Error: " + msg);
        return -1;
    }

    private static int helpMessage() {
        System.err.println("Usage:(All paramters below are required)");
        System.err.println("\tSlalom -t <seconds> -l <integer> -u <integer>");
        System.err.println("\tSlalom --time <seconds> --lower-bound integer "
                + "--upper-bound integer");
        return -1;
    }

    /** Returns parsed parameters, or null after printing an error/help. */
    private static Params commandLine(String[] argv) {
        /* No arguments specified. */
        if (argv.length == 0) {
            helpMessage();
            return null;
        }

        Params params = new Params();
        boolean sawTime = false;
        boolean sawLower = false;
        boolean sawUpper = false;
        for (int i = 0; i < argv.length; i++) {
            String arg = argv[i];
            String value = null;
            String option;
            if (arg.equals("-h") || arg.equals("--help")) {
                helpMessage();
                return null;
            } else if (arg.startsWith("--")) {
                int eq = arg.indexOf('=');
                if (eq >= 0) {
                    option = arg.substring(0, eq);
                    value = arg.substring(eq + 1);
                } else {
                    option = arg;
                }
            } else {
                option = arg;
            }

            if (value == null) {
                /* Option and value may also come as two arguments. */
                if (i + 1 >= argv.length) {
                    return null;
                }
                value = argv[++i];
            }

            try {
                switch (option) {
                case "-t":
                case "--time":
                    params.goal = Double.parseDouble(value);
                    sawTime = true;
                    break;
                case "-l":
                case "--lower-bound":
                    params.lBound = Integer.parseInt(value);
                    sawLower = true;
                    break;
                case "-u":
                case "--upper-bound":
                    params.uBound = Integer.parseInt(value);
                    sawUpper = true;
                    break;
                default:
                    errmsg("unknown option: " + option);
                    return null;
                }
            } catch (NumberFormatException e) {
                errmsg("Invalid or incomplete parameters:\n"
                        + "\ttime, lower-bound, upper-bound");
                return null;
            }
        }
        if (!(sawTime && sawLower && sawUpper)) {
            errmsg("Invalid or incomplete parameters:\n"
                    + "\ttime, lower-bound, upper-bound");
            return null;
        }
        if (params.goal <= 0 || params.lBound <= 0
                || params.uBound <= params.lBound) {
            errmsg("Invalid or incomplete parameters:\n"
                    + "\ttime, lower-bound, upper-bound");
            return null;
        }
        return params;
    }

    /**
     * Checks the lower bound provided by the user: the time taken to solve
     * the problem must be less than the goal.
     */
    private static boolean setLowerBound(double goal, int lowerBound) {
        MeterResult run = meter(lowerBound);
        if (!run.ok) {
            return false;
        }
        if (run.timing >= goal) {
            System.err.println("Must take less than " + g(goal) + " seconds. Took "
                    + g(run.timing) + ".");
            return false;
        }
        return true;
    }

    /**
     * Checks the upper bound provided by the user: the time taken to solve
     * the problem must be at least the goal.
     */
    private static boolean setUpperBound(double goal, int upperBound) {
        MeterResult run = meter(upperBound);
        if (!run.ok) {
            return false;
        }
        if (run.timing < goal) {
            System.err.println("Must take atleast " + g(goal) + " seconds. Took "
                    + g(run.timing) + ".");
            return false;
        }
        return true;
    }

    /**
     * Bisection over the problem size: while the [n, nupper] interval is
     * larger than 1, bisect it and pick a half.
     */
    private static boolean slalomMainLoop(double goal, int lowerBound,
            int upperBound) {
        while (upperBound - lowerBound > 1) {
            int mean = (lowerBound + upperBound) / 2;
            MeterResult run = meter(mean);
            if (!run.ok) {
                return false;
            }
            if (run.timing < goal) {
                lowerBound = mean;
            } else {
                upperBound = mean;
            }
            System.out.printf(LOC, "New interval: [%d,%d]\n", lowerBound, upperBound);
        }

        /* Ensure that the most recent run was for lowerBound. */
        MeterResult run = meter(lowerBound);
        if (!run.ok) {
            return false;
        }
        what(lowerBound, run.timing, run.work);
        return true;
    }

    public static void main(String[] argv) {
        Params params = commandLine(argv);
        if (params == null) {
            System.exit(1);
        }
        if (!setLowerBound(params.goal, params.lBound)) {
            System.exit(1);
        }
        if (!setUpperBound(params.goal, params.uBound)) {
            System.exit(1);
        }
        System.exit(slalomMainLoop(params.goal, params.lBound, params.uBound)
                ? 0 : 1);
    }
}
