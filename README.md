#  SLALOM
## Scalable Language-independent Ames Laboratory One-minute Measurement

This directory contains the source code for the non-optimized version of the
SLALOM benchmark for any uni processor system.  This the the plain-vanilla
version.  The only enhancement for speed is the function DDot() which
performs a double-precision dot-product.  At present, the only DDot()
implemented is for a Sun 4 (SPARC).  Other RISC systems' versions should be
very similar.

 ## Build
Please build the benchmark using the following command:
```
$ cd slalom
$ make 
```
The binary named slalom will be built.

## Running the Benchmark

``` $ ./slalom ```

The program will prompt for the amount of time you want to solve the surface rendering problem. The benchmark is scalable because it measures the capability of hardware to solve the amount of the problem. Not the other way round, as is the case with other benchmarks. An example is given below.
```
Enter the number of seconds that is the goal: 15
Enter a lower bound for n: 2000
```
The lower bound can be any integer between 0 to NMAX value. (default set to 4096). The upper bound will be the value between lower bound and NMAX.

Sample Results:
================
```
1.
-------------------------------------------------------------------
Machine:  Raspberry pi 3      Processor:  armv7l
Memory:   1 GB                # of procs: 1
Cache:                        # used:     1
NMAX:     4096                Clock:     700 MHz
Disk:     4GB Flash           Node name: Model 3 
OS:       raspbian lite
          GNU/Linux 4.9       Timer:      Wall, gettimeofday()
Language: C                   Alone:      yes
Compiler: gcc v4.9.2          Run by:     Naveen Sharma
Options:  -O3 -DSUN4          Date:       26th, August 2017
M ops:    13477               Time:       63.005 seconds
n:        2390                MFLOPS:     213.9
Approximate data memory use: 45984400 bytes.
-------------------------------------------------------------------
2.
Machine:  Raspberry pi 3      Processor:  armv7l
Memory:   1 GB                # of procs: 1
Cache:    16KB(L1)            # used:     1
NMAX:     4096                Clock:      700 MHz
Disk:     32GB Flash(Class10)
          Samsung EVO         Node name:  raspberry-n
OS:       Ubuntu Mate          Timer:      Wall, gettimeofday()
          GNU/Linux 4.4
Language: C                   Alone:      yes
Compiler: gcc v5.4            Run by:     Naveen Sharma
Options:  -O3 -DSUN4          Date:       27 August 2017
M ops:    12103.3             Time:       54.067 seconds
n:        2303                MFLOPS:     223.86
Approximate data memory use: 42707632 bytes.
-------------------------------------------------------------------
3.
Machine:  Raspberry pi 3       Processor:  armv7l
Memory:   1 GB                 # of procs: 1
Cache:    L1 16KB              # used:     1
NMAX:     4096                 Clock:      700 MHz
Disk:     32 GB Flash          Node name:  raspberry
OS:       Ubuntu Mate          Timer:      Wall, gettimeofday()
Language: C                    Alone:      yes
Compiler: gcc (5.4)            Run by:     Naveen Sharma
Options:  -O3                  Date:       27 August 2017
M ops:    9377.82              Time:       61.439 seconds
n:        2109                 MFLOPS:     152.64
Approximate data memory use: 35836928 bytes.
-------------------------------------------------------------------
4.
Machine:  64 Bit       Processor:  x86-64
Memory:   2 GB                 # of procs: 2
Cache:                         # used:     1
NMAX:     4096                 Clock:      3 GHz
Disk:     20 GB                Node name:  centos
OS:       Centos 7             Timer:      Wall, gettimeofday()
Language: C                    Alone:      yes
Compiler: gcc                  Run by:     Naveen Sharma
Options:  -O3                  Date:       03 May 2018
M ops:    44824.4              Time:       29.942 seconds
n:        3606                 MFLOPS:     1497.1

-------------------------------------------------------------------
5.
Machine:  Intel             Processor:  x86_64
Memory:   16 GB                # of procs: 4
Cache:    L1 256KB             # used:     1
NMAX:     4096                 Clock:      3.2 GHz
Disk:     100 GB              Node name:  dev-001 
OS:       Windows 10 with WSO  Timer:      Wall, gettimeofday()
Language: C                    Alone:      yes
Compiler: gcc (7.3)            Run by:     Naveen Sharma
Options:  -O3 -DSUN4           Date:       July 2018
M ops:    30671.5              Time:       14.864 seconds
n:        3168                 MFLOPS:     2063.5
Approximate data memory use: 80670752 bytes.

----------------------------------------------------------------------
6.
Machine:  Intel             Processor:  x86_64
Memory:   16 GB                # of procs: 4
Cache:    L1 256KB             # used:     1
NMAX:     8196              Clock:      3.2 GHz
Disk:     8 GB              Node name:  linux-dev-001
OS:       Linux Mint        Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.8)         Run by:     Naveen Sharma
Options:  -O3 -DSUN4        Date:       2018
M ops:    65619.3           Time:       30.539 seconds
n:        4105              MFLOPS:     2148.7
Approximate data memory use: 135301600 bytes.

-------------------------------------------------------------------
7.
Machine: ARMv7L              Processor:  32 bit
Memory:   1 GB               # of procs: 4
Cache:    L1 256KB           # used:     1
NMAX:    2048              Clock:      1.4 GHz
Disk:    32 GB             Node name:  raspberry-3b-pls
OS:      Ubuntu Mate 18.04  Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.4)         Run by:     Hemanya Sharma
Options:  -O2 -DSUN4        Date:       2019
M ops:    6472.54           Time:       29.818 seconds
n:        1855              MFLOPS:     217.06
Approximate data memory use: 27751600 bytes.
-------------------------------------------------------------------
8.
Machine: x86_64             Processor:  64 bit
Memory:  16 GB              # of procs: 8
Cache:                      # used:     1
NMAX:    8196               Clock:      1.4 GHz
Disk:    256 GB             Node name:  x86-dev
OS:      Ubuntu 18.04       Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.4)         Run by:     Naveen  Sharma
Options:  -O2 -DSUN4        Date:       2019
M ops:    59594.1              Time:       29.958 seconds
n:        3973                 MFLOPS:     1989.2
-------------------------------------------------------------------
9.
Machine: x86_64             Processor:  64 bit
Memory:  16 GB              # of procs: 4
Cache:                      # used:     1
NMAX:    8196               Clock:      2.71 GHz
Disk:    100 GB             Node name:  x86-dev
OS:Ubuntu 18.04.4 LTS    
  on Windows 10 x86_64     Timer:      Wall, gettimeofday()
Language: C                Alone:      yes
Compiler: gcc              Run by:     Naveen  Sharma
Options:  -O2 -DSUN4       Date:       2020
M ops:    25619.9              Time:       15.335 seconds
n:        2979                 MFLOPS:     1670.6
Approximate data memory use: 71353808 bytes.
---------------------------------------------------------------------
10.
Machine: x86_64             Processor:  64 bit
Memory:  16 GB              # of procs: 4
Cache:                      # used:     1
NMAX:    8196               Clock:      1.4 GHz
Disk:    256 GB             Node name:  x86-dev
OS: Debian buster           4.4.0-17763-Microsoft       Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.4)         Run by:     Naveen  Sharma
Options:  -O2 -DSUN4        Date:       2020
M ops:    26944.8              Time:       15.488 seconds
n:        3031                 MFLOPS:     1739.7
Approximate data memory use: 73860208 bytes.

```
