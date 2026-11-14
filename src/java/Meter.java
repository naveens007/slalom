import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.EnumMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Meter: times one complete run of the benchmark at problem size npatch,
 * from reading the geometry through storing the answer, and reports the
 * timing, analytic operation counts and speed of each phase.
 */
public final class Meter {

    /** Largest npatch for this computer; overridable for experiments. */
    public static final int NMAX = Integer.getInteger("SLALOM_NMAX", 8192);

    private Meter() {
    }

    /** Wall clock in seconds (like gettimeofday in the C reference). */
    private static double when() {
        return System.nanoTime() * 1e-9;
    }

    /**
     * Times one complete solve at problem size {@code npatch}.
     *
     * @return the result; {@code ok == false} when the run was rejected
     */
    public static Slalom.MeterResult meter(int npatch) {
        /* First check that npatch lies between 6 and NMAX: */
        if (npatch < 6) {
            System.out.println("Must be at least 6, the number of faces.");
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }
        if (npatch > NMAX) {
            System.out.printf(CFormat.LOC, "Exceeds %d = maximum for this system.\n", NMAX);
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }

        /* Ensure that a previous 'answer' file is deleted: */
        try {
            Files.deleteIfExists(Paths.get("answer"));
        } catch (IOException e) {
            /* Ignore; Storer overwrites it anyway. */
        }

        /* Time the tasks, individually and collectively.  The unpaired
         * timestamps are matched to the Task enum in declaration order. */
        List<Double> stamps = new ArrayList<Double>();
        stamps.add(when());
        Problem problem = Problem.read();
        if (problem == null) {
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }
        stamps.add(when());

        Geometry geometry = Geometry.decompose(npatch, problem);
        if (geometry == null) {
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }
        stamps.add(when());

        RadiositySystem system = new RadiositySystem(geometry);
        system.addParallelFormFactors();
        stamps.add(when());
        system.addOrthogonalFormFactors();
        stamps.add(when());
        if (!system.normalise()) {
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }
        stamps.add(when());
        system.solve();
        stamps.add(when());
        if (!Storer.store(geometry, system.result)) {
            return new Slalom.MeterResult(false, 0.0, 0.0);
        }
        stamps.add(when());

        double timing = stamps.get(stamps.size() - 1) - stamps.get(0);
        Map<Task, Double> seconds = new EnumMap<Task, Double>(Task.class);
        Iterator<Double> stampIterator = stamps.iterator();
        double previous = stampIterator.next();
        for (Task task : Task.values()) {
            double current = stampIterator.next();
            seconds.put(task, current - previous);
            previous = current;
        }

        /* Assess the floating-point work done by each routine, and total.
         * The counts are tallied into doubles to prevent integer overflow. */
        Map<Task, Double> ops = operationCounts(npatch, geometry);
        double work = 0.0;
        for (double value : ops.values()) {
            work += value;
        }

        /* Display the timing-work-speed breakdown by routine. */
        System.out.printf(CFormat.LOC, "%d patches:\n", npatch);
        System.out.print(" Task  Seconds       Operations        MFLOPS    % of Time\n");
        String format = "%6.6s%8.3f%17.0f%14.6f%10.1f %%\n";
        for (Task task : Task.values()) {
            double secs = seconds.get(task);
            double taskOps = ops.get(task);
            if (secs == 0.0) {
                secs = 0.001;
            }
            System.out.printf(CFormat.LOC, format, task.label, secs, taskOps,
                    (taskOps / secs) * 1e-6, 100.0 * secs / timing);
        }
        System.out.printf(CFormat.LOC, format, "TOTALS", timing, work,
                (work / timing) * 1e-6, 100.0);
        system.verify();

        return new Slalom.MeterResult(true, timing, work);
    }

    /**
     * The analytic LLNL-weighted operation count of each phase; the counts
     * follow ../slalom.h's weights and must not be changed.
     */
    private static Map<Task, Double> operationCounts(int npatch, Geometry geometry) {
        double[] box = geometry.box();
        double[] p = new double[6];
        int columns = 0;
        double sumSquares = 0.0;
        for (int i = 0; i < 6; i++) {
            p[i] = geometry.face(i).patchCount();
            sumSquares += p[i] * p[i];
            /* int addition truncates, exactly like the C reference. */
            columns += Math.sqrt(p[i] * box[i] / box[i + 1]) + 0.5;
        }
        double opposite = p[0] * p[3] + p[1] * p[4] + p[2] * p[5];
        int non0 = geometry.firstNonCoplanar();
        double non0Count = non0 + 1.0;

        Map<Task, Double> ops = new EnumMap<Task, Double>(Task.class);
        ops.put(Task.READER, 258.0);
        ops.put(Task.REGION, 154.0 + 8.0 * columns + npatch);
        ops.put(Task.SET_UP_1, 3.0 + 840.0 * opposite);
        ops.put(Task.SET_UP_2,
                430.0 * ((double) npatch * npatch - sumSquares - 2.0 * opposite) / 2.0);
        ops.put(Task.SET_UP_3,
                72.0 + 9.0 * npatch + (double) npatch * npatch - sumSquares);
        ops.put(Task.SOLVER,
                npatch * (npatch * ((double) npatch + 7.5) - 2.5) - 21.0
                + non0Count * (non0Count * (2.0 * non0Count - 16.5) + 35.5)
                + non0Count * npatch * (9.0 - 3.0 * non0Count));
        ops.put(Task.STORER, 48.0 * npatch);
        return ops;
    }
}
