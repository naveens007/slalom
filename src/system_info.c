#include <stdio.h>
#include <time.h>
#include <cpuinfo.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include "slalom.h"
#include "system_info.h"


const char *determine_compiler ();
const char *determine_os ();
char *benchmark_date_time ();
char *processor_info ();
double total_ram ();
double free_ram ();
const char *get_build();
char *cuserid (char *);


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
  // getlogin_r (u_name, BUFFER_SIZE);
  cuserid(u_name);
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
  if (!cpuinfo_initialize()) 
  {
     fprintf(stderr, "failed to initialize CPU information\n");
     machine_info.proc = "unknown";
  }
  else
  {
    const struct cpuinfo_package *p_package = cpuinfo_get_package(0); 
    machine_info.proc  = p_package->name;
    machine_info.cores = p_package->core_count;
    const struct cpuinfo_processor* proc  =  cpuinfo_get_current_processor();
    machine_info.clock_speed = proc->core->frequency;
    machine_info.l1i_cache = (proc->cache.l1i->size)/kilo_bytes;
    machine_info.l2_cache = (proc->cache.l2->size)/kilo_bytes;
  }
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
    printf("Machine:%s\t\t Processor:  %s\n"             , p_info->arch, p_info->proc);
    printf("Memory:(%5.2f/%5.2f) GB\t # of procs: %d\n"  , p_info->used_ram, p_info->total_ram, p_info->cores);
    printf("L1iCache:%d(KB)\t\t # used:1\n"            , p_info->l1i_cache);
    printf("NMAX:%5d \t\t Clock:%5.2fGHz\n"         ,p_info->nmax, p_info->clock_speed);
    printf("OS:%s\t\t Timer:Wall, gettimeofday()\n" ,p_info->os_string);
    printf("L2 Cache:%d KB\t Node name: %s\n"     ,p_info->l2_cache, p_info->node_name);
    printf("Language:C\t\t Version:%s\n"            ,p_info->version);
    printf("Compiler:%s\t\t Run by:%s\n"            ,p_info->compiler, p_info->user_name);
    printf("Options:-O2 -DSUN4\t Date:%s\n"         ,p_info->datetime);
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
#elif defined(__unix_)_
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

    snprintf(date_time, BUFFER_SIZE, "%02d/%02d/%04d (dd/mm/yyyy) %02d:%02d:%02d",\
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

 ram = (si.totalram/giga_bytes);
 return ram;
}

double free_ram ()
{
 double ram = 0;

 /* Obtain system statistics. */
 struct sysinfo si;
 sysinfo (&si);
 ram =  si.freeram/giga_bytes;
 return ram;
}

    const char *get_build ()
    { //Get current architecture, detectx nearly every architecture. Coded by Freak
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
        return "ARM5"
        #elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
        return "ARM6T2";
        #elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
        return "ARM6";
        #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7";
        #elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7A";
        #elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7R";
        #elif defined(__ARM_ARCH_7M__)
        return "ARM7M";
        #elif defined(__ARM_ARCH_7S__)
        return "ARM7S";
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
