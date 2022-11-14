
/* Avoid multiple inclusions  */
#ifndef __SYSINFO_H_INCLUDE__
#define __SYSINFO_H_INCLUDE__

#define BUFFER_SIZE 64

typedef struct
{
    /* Hostname                */
    const char *node_name;
#ifdef NMAX
    /* Specific NMAX value for SLALOM benchmark. */
    int        nmax;
#endif
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

#endif
