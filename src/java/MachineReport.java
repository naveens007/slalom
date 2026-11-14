import java.io.BufferedReader;
import java.io.IOException;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

/**
 * Machine information, the Java counterpart of ../system_info.c, plus the
 * final statistics block (What in the C reference).
 */
final class MachineReport {

    private MachineReport() {
    }

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
                        && kind.toLowerCase(CFormat.LOC).startsWith("instruction")) {
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
        String date = String.format(CFormat.LOC,
                "%02d/%02d/%04d (dd/mm) %02d:%02d:%02d",
                now.getDayOfMonth(), now.getMonthValue(), now.getYear(),
                now.getHour(), now.getMinute(), now.getSecond());

        System.out.printf(CFormat.LOC, "Machine:%s\t\t Processor:  %s\n", arch, proc);
        System.out.printf(CFormat.LOC, "Memory: %3.2f%% \t\t # of procs: %d\n", memUsage, cores);
        System.out.printf(CFormat.LOC, "L1 iCache:%d KB\t\t # used:1\n", caches[0]);
        if (clock > 0.0) {
            System.out.printf(CFormat.LOC, "NMAX:%5d \t\t Clock:%5.2fGHz\n", Meter.NMAX, clock);
        } else {
            /* The frequency cannot always be determined; do not pretend
             * it is 0.00GHz. */
            System.out.printf(CFormat.LOC, "NMAX:%5d \t\t Clock:   N/A\n", Meter.NMAX);
        }
        System.out.printf(CFormat.LOC, "OS:%s\t\t Timer:Wall, System.nanoTime()\n", os);
        System.out.printf(CFormat.LOC, "L2 Cache:%d KB\t\t Node: %s\n", caches[1], node);
        System.out.printf(CFormat.LOC, "Language:Java\t\t Version:%s\n", version);
        System.out.printf(CFormat.LOC, "Compiler:javac\t\t Run by:%s\n", user);
        System.out.printf(CFormat.LOC, "Options:standard Java\t Date:%s\n", date);
    }

    /** Prints the machine report and the final statistics. */
    static void what(int n, double timing, double work) {
        System.out.print("\n");
        systemInfo();

        System.out.print("\n");
        System.out.printf(CFormat.LOC, "M ops:    %-13s        Time:       %.3f seconds\n",
                CFormat.g(work * 1e-6), timing);
        System.out.printf(CFormat.LOC, "n:        %-6d               MFLOPS:     %s\n",
                n, CFormat.g((work / timing) * 1e-6, 5));
        System.out.printf(CFormat.LOC, "Approximate data memory use: %d bytes.\n",
                8L * n * n + 120L * n + 800);
    }
}
