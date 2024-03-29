#  SLALOM
## Scalable Language-independent Ames Laboratory One-minute Measurement

This directory contains the source code for the non-optimized version of the
SLALOM benchmark for any uni processor system. It means that this benchmark can tell
performance for that processor, without needing any external data.
This the the plain-vanilla version.  The only enhancement for speed is the function 
DDot() which performs a double-precision dot-product.  At present, the only DDot()
implemented is for a typical RISC architecture.

The benchmark will primarily test instruction shcheduling of an processor 
architecture.

 ## Build
Please build the benchmark using the following command:
```
$ cmake -S /path/to/source -B /path/to/build_directory
$ make -C /path/to/build_directory
```
The binary named slalom will be built at /path/to/build_directory/bin

## Running the Benchmark

``` $ ./slalom  -t 10 -l 400 -u 4000```

The -t option specifies amount of time you want to solve the surface rendering problem. The benchmark is scalable because it measures the capability of hardware to solve the amount of the problem. Not the other way round, as is the case with other benchmarks. 

The lower bound can be any integer between 0 to NMAX value. (default set to 4096). 
The upper bound will be the value greater than lower bound and but below NMAX (which is currently defined as 8192).

Sample Results:
================
```
Machine:x86_64           Processor:  Intel 11th Gen Core i7-1165G7
Memory: 91.71%           # of procs: 4
L1 iCache:32 KB          # used:1
NMAX: 8192               Clock: 2.80GHz
OS:Linux                 Timer:Wall, gettimeofday()
L2 Cache:1280 KB         Node name: DESKTOP-NMTUG6U
Language:C               Version:Clang 15.0.7 (https://github.com/llvm/llvm-project.git 8dfdcc7b7bf66834a761bd8de445840ef68e4d1a)
Compiler:clang           Run by:naveens0
Options:-O2 -DSUN4       Date:26/02/2023 (dd/mm) 23:42:25

M ops:    83332.9              Time:       32.736 seconds
n:        4452                 MFLOPS:     2545.6
Approximate data memory use: 159097472 bytes.
-------------------------------------------------------------------
Machine:  Raspberry pi 3      Processor:  armv7l
Memory:   1 GB                # of procs: 1
Cache:                        # used:     1
NMAX:     4096                Clock:     700 MHz
Disk:   4GB microSD (Class 4) Node name: Model 3 
OS:       raspbian lite
          GNU/Linux 4.9       Timer:      Wall, gettimeofday()
Language: C                   Alone:      yes
Compiler: gcc v4.9.2          Run by:     Naveen Sharma
Options:  -O3 -DSUN4          Date:       26th, August 2017
M ops:    13477               Time:       63.005 seconds
n:        2390                MFLOPS:     213.9
Approximate data memory use: 45984400 bytes.
-------------------------------------------------------------------

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
Machine:  Raspberry pi 3       Processor:  armv7l
Memory:   1 GB                 # of procs: 1
Cache:    L1 16KB              # used:     1
NMAX:     4096                 Clock:      700 MHz
Disk:     32 GB Flash          Node name:  raspberry
OS:       Ubuntu Mate 18.04    Timer:      Wall, gettimeofday()
Language: C                    Alone:      yes
Compiler: gcc (5.4)            Run by:     Naveen Sharma
Options:  -O3                  Date:       27 August 2017
M ops:    9377.82              Time:       61.439 seconds
n:        2109                 MFLOPS:     152.64
Approximate data memory use: 35836928 bytes.
-------------------------------------------------------------------
Machine:  64 Bit               Processor:  x86-64
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
Machine:  Intel             Processor:  x86_64
Memory:   16 GB             # of procs: 4
Cache:    L1 256KB          # used:     1
NMAX:     4096              Clock:      3.2 GHz
Disk:     100 GB            Node name:  dev-001 
OS:       Windows10(WSL)    Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.3)         Run by:     Naveen Sharma
Options:  -O3 -DSUN4        Date:       July 2018
M ops:    30671.5           Time:       14.864 seconds
n:        3168              MFLOPS:     2063.5
Approximate data memory use: 80670752 bytes.
----------------------------------------------------------------------
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
Machine: ARMv7L            Processor:  32 bit
Memory:   1 GB             # of procs: 4
Cache:    L1 256KB         # used:     1
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
Machine: x86_64             Processor:  64 bit
Memory:  16 GB              # of procs: 8
Cache:                      # used:     1
NMAX:    8196               Clock:      1.4 GHz
Disk:    256 GB             Node name:  x86-dev
OS:      Ubuntu 18.04       Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.4)         Run by:     Naveen  Sharma
Options:  -O2 -DSUN4        Date:       2019
M ops:    59594.1           Time:       29.958 seconds
n:        3973              MFLOPS:     1989.2
-------------------------------------------------------------------
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
---------------------------------------------------------------------
Machine: x86_64             Processor:  64 bit i5-7200U
Memory:  16 GB              # of procs: 4
Cache:                      # used:     1
NMAX:    8196               Clock:      3.1 GHz
Disk:    USB3.0 8 GB        Node name:  x86-dev
OS:  Pop!_OS 20.04 live USB Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc (7.4)         Run by:     Naveen  Sharma
Options:  -O2 -DSUN4        Date:       2020
M ops:    22743.4           Time:       14.827 seconds
n:        2860              MFLOPS:     1533.9
Approximate data memory use: 65780800 bytes.
---------------------------------------------------------------------
Machine: x86_64             Processor:  64 bit i7-9750H
Memory:  16 GB              # of procs: 12
Cache:                      # used:     1
NMAX:    8196               Clock:      3.1 GHz
Disk:    512 GB  NVMe       Node name:  x86-dev
OS:    Ubuntu Budgie 20.04  Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc               Run by:     Naveen  Sharma
Options:  -O2 -DSUN4        Date:       23 May 2020
M ops:    35805.1           Time:       15.665 seconds
n:        3340              MFLOPS:     2285.7
Approximate data memory use: 89646400 bytes.
----------------------------------------------------------------------
Machine: x86_64             Processor:  64 bit i7-9750H
Memory:  16 GB              # of procs: 6 
Cache:                      # used:     1
NMAX:    8196               Clock:      3.1 GHz
Disk:    512 GB NVMe        Node name:  x86-dev
OS:      Pop!_OS 20.04      Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc               Run by:     Naveen SHarma
Options:  -O2 -DSUN4        Date:       July 2020
M ops:    36916.7           Time:       15.618 seconds
n:        3375              MFLOPS:     2363.7
Approximate data memory use: 91530800 bytes.
----------------------------------------------------------------------
Machine: x86_64             Processor:  Intel i7-9750H
memory:  16 GB              # of procs: 6
Cache:   12M                # used:     1
NMAX:    8196               Clock:      2.6 GHz
Disk:    512 GB NVMe        Node name:  x86-dev
OS:      Pop!_OS 21.04      Timer:      Wall, gettimeofday()
Language: C                 Alone:      yes
Compiler: gcc               Run by:     Naveen Sharma
Options:  -O2 -DSUN4        Date:       July 2021
M ops:    73387.8           Time:       30.250 seconds
n:        4264              MFLOPS:     2426
Approximate data memory use: 145966048 bytes.
----------------------------------------------------------------------
Mchine: x86_64             Processor:  Intel i5-7200U
Memory:  16 GB              # of procs: 4
Cache:   12M                # used:     1
NMAX:    8196               Clock:      2.712 GHz
Disk:    512 GB             Node name:  DLLT-6197
OS:      Windows 10         Timer:      Wall, gettimeofday()Language: C                 Alone:      yes
Compiler: gcc               Run by:     Naveen Sharma
Options:  -O2 -DSUN4        Date:       Sept 2021
M ops:    120500            Time:       57.135 seconds
n:        5045              MFLOPS:     2109
Approximate data memory use: 204222400 bytes
----------------------------------------------------------------------
Machine:x86_64           Processor:  Intel 11th Gen Core i7-1165G7
Memory: 90.34%           # of procs: 4
L1 iCache:32 KB          # used:1
NMAX: 8192               Clock: 2.80GHz
OS:Linux                 Timer:Wall, gettimeofday()
L2 Cache:1280 KB         Node name: DESKTOP-NMTUG6U
Language:C               Version:9.4.0
Compiler:gcc             Run by:naveens
Options:-O2 -DSUN4       Date:26/02/2023 (dd/mm) 23:12:45

M ops:    71023.1              Time:       29.329 seconds
n:        4217                 MFLOPS:     2421.6
Approximate data memory use: 142771552 bytes.
----------------------------------------------------------------------
```
