#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/sysinfo.h>

#ifdef SLALOM_HAVE_CPUINFO
#include <cpuinfo.h>
#endif

#include "slalom.h"
#include "system_info.h"


const char *determine_compiler ();
const char *determine_os ();
char *benchmark_date_time ();
char *processor_info ();
double total_ram ();
double free_ram ();
const char *get_build ();
const char *build_options ();

/* Forward declarations of the internal detection helpers. */
static void   detect_processor (MachineInfo *p_info);
static double detect_clock_speed ();
static const char *detect_processor_name ();
static int    read_first_field (const char *path, const char *key,
                                char *out, size_t outlen);
static int    read_text_file (const char *path, char *out, size_t outlen);
static void   get_user_name (char *out, size_t outlen);
#ifdef SLALOM_HAVE_CPUINFO
static const char *vendor_name (enum cpuinfo_vendor vendor);
#endif


/* Conversion Constatnts. */
static const double giga_bytes = 1024 * 1024 * 1024;
static const double mega_bytes = 1024 * 1024;
static const double kilo_bytes = 1024;

static MachineInfo machine_info = {0};
static char date_time [BUFFER_SIZE] = {0};
static char host_name [BUFFER_SIZE] = {0};
static char u_name [BUFFER_SIZE]    = {0};

MachineInfo *get_system_info ()
{
    gethostname (host_name, BUFFER_SIZE);
    get_user_name (u_name, BUFFER_SIZE);
    machine_info.node_name = host_name;
    machine_info.user_name = u_name;
    machine_info.arch = get_build ();
#ifdef NMAX
    machine_info.nmax = NMAX;
#endif
    machine_info.os_string = determine_os ();
    machine_info.compiler = determine_compiler ();
    machine_info.version = __VERSION__;
    machine_info.datetime = benchmark_date_time ();

    /* Processor identification. Uses libcpuinfo when available and falls back
     * to /proc/cpuinfo (and the device tree) otherwise. */
    detect_processor (&machine_info);

    machine_info.total_ram = total_ram ();
    machine_info.used_ram  = machine_info.total_ram - free_ram ();

    return &machine_info;
}

int system_info (MachineInfo *p_info)
{
#if 0
    static const char *info[] =
    {
        "Machine: %s         Processor:  %s",
        "Memory:  %f GB      # of procs: %d",
        "L1-Cache:   %d         # used:     1",
        "NMAX:    %d         Clock:    %fGHz",
        "Disk:    %d GB      Node name: %s  ",
        "OS:      %s         Timer:      Wall, gettimeofday()",
        "Language: C         Alone:      yes",
        "Compiler: %s        Run by:     %s",
        "Options:  -O2 -DSUN4        Date:%s",
        NULL
    };
#endif
    double mem_usage = (p_info->used_ram/p_info->total_ram) * 100;
    printf("Machine:%s\t\t Processor:  %s\n", p_info->arch, p_info->proc);
    printf("Memory: %3.2f%% \t\t # of procs: %d\n", mem_usage, p_info->cores);
    printf("L1 iCache:%d KB\t\t # used:1\n", p_info->l1i_cache);
    if (p_info->clock_speed > 0.0)
        printf("NMAX:%5d \t\t Clock:%5.2fGHz\n",p_info->nmax, p_info->clock_speed);
    else
        /* cpuinfo/ACPI cannot always determine the frequency (common on
         * many ARM SoCs); do not pretend it is 0.00GHz. */
        printf("NMAX:%5d \t\t Clock:   N/A\n",p_info->nmax);
    printf("OS:%s\t\t Timer:Wall, gettimeofday()\n",p_info->os_string);
    printf("L2 Cache:%d KB\t\t Node: %s\n",p_info->l2_cache, p_info->node_name);
    printf("Language:C\t\t Version:%s\n",p_info->version);
    printf("Compiler:%s\t\t Run by:%s\n",p_info->compiler, p_info->user_name);
    printf("Options:%s\t Date:%s\n", build_options (), p_info->datetime);
    return 0;
}

const char *determine_compiler ()
{
#if defined(__clang__)
    return "clang";
#elif defined(__INTEL_COMPILER)
    return "icc";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(__GNUG__)
    return "g++";
#elif defined(_MSC_VER)
    return "msvc";
#else
    return "unknown";
#endif
}

/**
 * Describe the configuration the benchmark was actually built with. The banner
 * used to print a hard-coded "-O2 -DSUN4" regardless of the real flags.
 */
const char *build_options ()
{
    static char options[BUFFER_SIZE];
#if defined(SUN4)
    const char *ddot = "SUN4/unrolled DDot";
#else
    const char *ddot = "generic DDot";
#endif
#ifndef SLALOM_BUILD_TYPE
#define SLALOM_BUILD_TYPE "unknown"
#endif
    snprintf (options, sizeof (options), "%s, %s", ddot,
              (SLALOM_BUILD_TYPE[0] != '\0') ? SLALOM_BUILD_TYPE : "no-build-type");
    return options;
}

const char *determine_os ()
{
#if defined(_WIN32)
    return "MS WINDOWS win32";
#elif defined(_WIN64)
    return "MS WINDOWS win64";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__MACH__)
    return "Appled Mach OS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__OpenBSD__)
    return "Open BSD";
#elif defined(__unix__)
    return "Unix";
#else
    return "unknown";
#endif
}

#ifdef STANDALONE
int main ()
{
    MachineInfo *info_ptr = get_system_info ();
    system_info (info_ptr);
    return 0;
}
#endif

char *benchmark_date_time ()
{

    time_t T = time(0);
    struct tm tm = *localtime(&T);

    snprintf(date_time, BUFFER_SIZE, "%02d/%02d/%04d (dd/mm) %02d:%02d:%02d",\
             tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,\
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return date_time;
}

char *processor_info ()
{
    return ("unknown");
}

double total_ram ()
{
    double ram = 0;

    /* Obtain system statistics. */
    struct sysinfo si;
    sysinfo (&si);

    /* totalram is expressed in mem_unit bytes, not bytes. */
    ram = ((double) si.totalram * si.mem_unit) / giga_bytes;
    return ram;
}

double free_ram ()
{
    double ram = 0;

    /* Obtain system statistics. */
    struct sysinfo si;
    sysinfo (&si);

    ram = ((double) si.freeram * si.mem_unit) / giga_bytes;
    return ram;
}

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/**
 * Read the value of "key : value" from a text file (e.g. /proc/cpuinfo).
 * Returns non-zero and a NUL terminated string in @out when found.
 */
static int read_first_field (const char *path, const char *key,
                             char *out, size_t outlen)
{
    FILE *f = fopen (path, "r");
    char line[256];
    size_t klen = strlen (key);

    if (f == NULL)
        return 0;

    while (fgets (line, sizeof (line), f) != NULL)
        {
            if (strncmp (line, key, klen) == 0)
                {
                    char *value = strchr (line, ':');
                    size_t n;
                    if (value == NULL)
                        break;
                    value++;
                    while (*value == ' ' || *value == '\t')
                        value++;
                    n = strcspn (value, "\r\n");
                    if (n >= outlen)
                        n = outlen - 1;
                    memcpy (out, value, n);
                    out[n] = '\0';
                    fclose (f);
                    return out[0] != '\0';
                }
        }
    fclose (f);
    return 0;
}

/**
 * Read a whole (usually one line) text file, stripping trailing NULs and
 * line terminators. Used for the device-tree "model" file.
 */
static int read_text_file (const char *path, char *out, size_t outlen)
{
    FILE *f = fopen (path, "rb");
    size_t n;
    char *nul;

    if (f == NULL)
        return 0;

    n = fread (out, 1, outlen - 1, f);
    fclose (f);
    out[n] = '\0';

    /* The device tree strings are NUL padded. */
    nul = strchr (out, '\0');
    if (nul != NULL)
        *nul = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == '\0'))
        out[--n] = '\0';

    return n > 0;
}

/**
 * Best-effort CPU frequency in GHz. Returns 0.0 when no source is available.
 */
static double detect_clock_speed ()
{
    static const char *freq_files[] =
    {
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
        NULL
    };
    char buf[64];
    int i;

    for (i = 0; freq_files[i] != NULL; i++)
        {
            if (read_text_file (freq_files[i], buf, sizeof (buf)))
                {
                    double khz = strtod (buf, NULL);
                    if (khz > 0.0)
                        return khz / 1.0e6;     /* kHz -> GHz */
                }
        }

    /* x86 exposes "cpu MHz"; some ARM kernels expose "clock" in Hz. */
    if (read_first_field ("/proc/cpuinfo", "cpu MHz", buf, sizeof (buf)))
        {
            double mhz = strtod (buf, NULL);
            if (mhz > 0.0)
                return mhz / 1.0e3;             /* MHz -> GHz */
        }
    if (read_first_field ("/proc/cpuinfo", "clock", buf, sizeof (buf)))
        {
            double hz = strtod (buf, NULL);
            if (hz > 0.0)
                return hz / 1.0e9;              /* Hz -> GHz */
        }

    return 0.0;
}

/**
 * Best-effort human readable SoC / processor name from the device tree or
 * /proc/cpuinfo. Returns NULL when nothing useful is found.
 */
static const char *detect_processor_name ()
{
    static char name[128];

    if (read_text_file ("/sys/firmware/devicetree/base/model", name, sizeof (name)))
        return name;
    if (read_first_field ("/proc/cpuinfo", "Hardware", name, sizeof (name)))
        return name;
    if (read_first_field ("/proc/cpuinfo", "model name", name, sizeof (name)))
        return name;
    if (read_first_field ("/proc/cpuinfo", "cpu model", name, sizeof (name)))
        return name;

    return NULL;
}

#ifdef SLALOM_HAVE_CPUINFO
/**
 * Short human readable vendor name. Returns NULL for vendors we do not map.
 * The microarchitecture name is not part of libcpuinfo's public API, so this
 * is the best CPU label available when no package/SoC name is reported.
 */
static const char *vendor_name (enum cpuinfo_vendor vendor)
{
    switch (vendor)
        {
        case cpuinfo_vendor_intel:            return "Intel";
        case cpuinfo_vendor_amd:              return "AMD";
        case cpuinfo_vendor_arm:              return "ARM";
        case cpuinfo_vendor_qualcomm:         return "Qualcomm";
        case cpuinfo_vendor_apple:            return "Apple";
        case cpuinfo_vendor_samsung:          return "Samsung";
        case cpuinfo_vendor_nvidia:           return "NVIDIA";
        case cpuinfo_vendor_huawei:           return "HiSilicon/Huawei";
        case cpuinfo_vendor_marvell:          return "Marvell";
        case cpuinfo_vendor_cavium:           return "Cavium";
        case cpuinfo_vendor_broadcom:         return "Broadcom";
        case cpuinfo_vendor_ibm:              return "IBM";
        case cpuinfo_vendor_hygon:            return "Hygon";
        case cpuinfo_vendor_apm:              return "Ampere/APM";
        case cpuinfo_vendor_texas_instruments:return "Texas Instruments";
        case cpuinfo_vendor_mips:             return "MIPS";
        default:                              return NULL;
        }
}
#endif

/**
 * Populate the processor fields of @p_info: name, core count, clock speed and
 * the L1i/L2 cache sizes. Prefers libcpuinfo and degrades gracefully.
 */
static void detect_processor (MachineInfo *p_info)
{
    const char *name = NULL;

    p_info->proc = "Unknown";
    p_info->cores = 0;
    p_info->clock_speed = 0.0;
    p_info->l1i_cache = 0;
    p_info->l2_cache = 0;

#ifdef SLALOM_HAVE_CPUINFO
    if (cpuinfo_initialize ())
        {
            const struct cpuinfo_package *package = cpuinfo_get_package (0);
            const struct cpuinfo_processor *processor = cpuinfo_get_current_processor ();

            if (package != NULL && package->name[0] != '\0'
                && strcmp (package->name, "Unknown") != 0)
                name = package->name;

            if (package != NULL)
                p_info->cores = (int) package->core_count;

            if (processor != NULL && processor->core != NULL)
                {
                    /* cpuinfo reports the frequency in Hz; the MachineInfo
                     * field is in GHz. */
                    p_info->clock_speed = processor->core->frequency / 1.0e9;
                    if (processor->cache.l1i != NULL)
                        p_info->l1i_cache = (int) (processor->cache.l1i->size / kilo_bytes);
                    if (processor->cache.l2 != NULL)
                        p_info->l2_cache = (int) (processor->cache.l2->size / kilo_bytes);
                }
        }
#endif

    /* Fallbacks for whatever cpuinfo could not provide. */
    if (p_info->cores <= 0)
        {
            long online = sysconf (_SC_NPROCESSORS_ONLN);
            p_info->cores = (online > 0) ? (int) online : 1;
        }
    if (p_info->clock_speed <= 0.0)
        p_info->clock_speed = detect_clock_speed ();
    if (name == NULL || name[0] == '\0' || strcmp (name, "Unknown") == 0)
        name = detect_processor_name ();
#ifdef SLALOM_HAVE_CPUINFO
    if (name == NULL || name[0] == '\0' || strcmp (name, "Unknown") == 0)
        {
            /* Last resort: several SoCs (e.g. Kunpeng-920) report no package
             * name, so derive a label from the reported vendor. */
            static char vendor_label[128];
            const struct cpuinfo_core *core = cpuinfo_get_core (0);
            const char *vendor = (core != NULL) ? vendor_name (core->vendor) : NULL;
            if (vendor != NULL)
                {
                    snprintf (vendor_label, sizeof (vendor_label), "%s %s",
                              vendor, get_build ());
                    name = vendor_label;
                }
        }
#endif
    if (name != NULL && name[0] != '\0')
        p_info->proc = name;
}

/**
 * Current user name. cuserid() truncates to 8 characters and is obsolete, so
 * prefer getlogin_r() with a getpwuid() fallback.
 */
static void get_user_name (char *out, size_t outlen)
{
    if (getlogin_r (out, outlen) != 0 || out[0] == '\0')
        {
            struct passwd *pw = getpwuid (getuid ());
            if (pw != NULL && pw->pw_name != NULL)
                snprintf (out, outlen, "%s", pw->pw_name);
            else
                snprintf (out, outlen, "unknown");
        }
}

const char *get_build ()
{
    //Get current architecture, detectx nearly every architecture. Coded by Freak
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return "x86_32";
#elif defined(__ARM_ARCH_2__)
    return "ARM2";
#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
    return "ARM3";
#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
    return "ARM4T";
#elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
    return "ARM5";
/* The more specific ARMv7 variants must be tested before the generic ARMv7
 * one, otherwise they are unreachable. */
#elif defined(__ARM_ARCH_7A__)
    return "ARM7A";
#elif defined(__ARM_ARCH_7R__)
    return "ARM7R";
#elif defined(__ARM_ARCH_7M__)
    return "ARM7M";
#elif defined(__ARM_ARCH_7S__)
    return "ARM7S";
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "ARM7";
#elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
    return "ARM6T2";
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
    return "ARM6";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(mips) || defined(__mips__) || defined(__mips)
    return "MIPS";
#elif defined(__sh__)
    return "SUPERH";
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
    return "POWERPC";
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
    return "POWERPC64";
#elif defined(__sparc__) || defined(__sparc)
    return "SPARC";
#elif defined(__m68k__)
    return "M68K";
#else
    return "unknown";
#endif
}
