
/* Avoid multiple inclusions  */
#ifndef __SYSINFO_H_INCLUDE__
#define __SYSINFO_H_INCLUDE__

/* This header is also included from the C++ implementation (src/cpp). */
#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 64

typedef struct
{
    /* Hostname                */
    const char *node_name;
    /* Specific NMAX value for SLALOM benchmark.

       Declared unconditionally: wrapping this field in #ifdef NMAX made the
       struct layout depend on whether the including translation unit happened
       to include slalom.h first (112 bytes with NMAX, 104 without), so a
       second implementation that did not see NMAX read every field at the
       wrong offset. Fields must never come and go with a preprocessor macro. */
    int        nmax;
    /* Operating System Type    */
    const char *os_string;
    /* Processor Architecture   */
    const char *arch;
    /* Total RAM Available      */
    double     total_ram;
    /* Free RAM Available       */
    double     used_ram;
    /* Available L2 Data Cache(KB)  */
    int     l2_cache;
    /* Processor Name and Type  */
    const char *proc;
    /* Number of cores in the processor. */
    int        cores;
    /* The L1i Cache on each core(KB)     */
    int     l1i_cache;
    /* Processor clock speed(GHz)        */
    double     clock_speed;
    /* Compiler used for the build       */
    const char *compiler;
    /* Compiler version used for the build    */
    const char *version;
    /* Date and Time when finished            */
    const char *datetime;
    /* The user name who ran the benchmark    */
    const char *user_name;
} MachineInfo;

/* Interface Functions.              */
MachineInfo * get_system_info ();
int system_info (MachineInfo *p_info);

#ifdef __cplusplus
}
#endif

#endif
