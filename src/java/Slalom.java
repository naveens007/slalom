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
 * The port is organised around the problem domain instead of the C arrays:
 *
 *   CFormat         printf-compatible number formatting (%g)
 *   Channel         the R/G/B colour channels
 *   Patch, Face     the surface decomposition's elements
 *   Problem         the description read from the 'geom' file
 *   Geometry        the box subdivided into patches (Iterable over patches)
 *   RadiositySystem the linear system: form factors, solve, residual check
 *   Storer          the 'answer' file writer
 *   Task            the seven timed phases
 *   Meter           one timed run at a given problem size
 *   MachineReport   the machine information and final statistics block
 *   Slalom          the fixed-time bisection driver (this class)
 *
 * The 'answer' file is written to the current working directory, matching
 * the C convention.
 *
 * John Gustafson, Diane Rover, Michael Carter, and Stephen Elbert
 * Ames Laboratory, 3/18/90 (reference C implementation)
 *****************************************************************************/
public final class Slalom {

    private Slalom() {
    }

    /** Result of Meter(); ok is false when the run was rejected. */
    public static final class MeterResult {
        public final boolean ok;
        public final double timing;
        public final double work;

        MeterResult(boolean ok, double timing, double work) {
            this.ok = ok;
            this.timing = timing;
            this.work = work;
        }
    }

    /** Times one complete run of the benchmark at size npatch. */
    public static MeterResult meter(int npatch) {
        return Meter.meter(npatch);
    }

    /* ------------------------------------------------------------------ */
    /*  Driver (the Java counterpart of sl-driver.c).                      */
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
            System.err.println("Must take less than " + CFormat.g(goal) + " seconds. Took "
                    + CFormat.g(run.timing) + ".");
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
            System.err.println("Must take atleast " + CFormat.g(goal) + " seconds. Took "
                    + CFormat.g(run.timing) + ".");
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
            System.out.printf(CFormat.LOC, "New interval: [%d,%d]\n", lowerBound, upperBound);
        }

        /* Ensure that the most recent run was for lowerBound. */
        MeterResult run = meter(lowerBound);
        if (!run.ok) {
            return false;
        }
        MachineReport.what(lowerBound, run.timing, run.work);
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
