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

### Optional: the C++ implementation

`src/slalom.c` is the frozen reference implementation — it is what produces
the published numbers and it is not being refactored. A C++ port lives in
`src/cpp/` and is **off by default**, so a normal build produces exactly the
same binaries as before:

```
$ cmake -S /path/to/source -B /path/to/build_directory -DSLALOM_ENABLE_CXX=ON
$ make -C /path/to/build_directory
```

This additionally builds `slalom-cxx` next to `slalom`. The C++ variant uses
`std::vector`-based dense matrices and a direct Gaussian-solver to exercise the
same radiosity-style linear system with C++ containers. It is intentionally kept
opt-in so the default build remains bit-for-bit equivalent to the published C
reference.

## Running the Benchmark

``` $ ./slalom  -t 10 -l 400 -u 4000```

The -t option specifies amount of time you want to solve the surface rendering problem. The benchmark is scalable because it measures the capability of hardware to solve the amount of the problem.

The lower bound can be any integer between 0 to NMAX value. (default set to 4096).
The upper bound will be the value greater than lower bound and but below NMAX (which is currently defined as 8192).

Sample Results:
===============
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
```
