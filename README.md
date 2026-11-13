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

### Optional: the C++ implementation (scaffold)

`src/slalom.c` is the frozen reference implementation — it is what produces
the published numbers and it is not being refactored. A C++ port lives in
`src/cpp/` and is **off by default**, so a normal build produces exactly the
same binaries as before:

```
$ cmake -S /path/to/source -B /path/to/build_directory -DSLALOM_ENABLE_CXX=ON
$ make -C /path/to/build_directory
```

This additionally builds `slalom-cxx` next to `slalom`. At this stage
`slalom-cxx` only verifies the build plumbing (RAII file handling, linkage
against the shared C machine-report code) and deliberately prints **no
benchmark result** — the solver phases are not ported yet.

## How SLALOM Measures Performance

### The problem it solves: radiosity inside a box

SLALOM does not time a synthetic kernel. It solves a complete optical
radiosity problem: a closed box has reflectivity and emissivity on each of
its six inner faces, and the benchmark computes the equilibrium distribution
of light bouncing between those surfaces. The result is the Red-Green-Blue
radiosity of every surface patch — the same quantity a renderer needs to
shade the inside of the box.

The input is the `geom` file (`13.5 x 9.0 x 8.0` box by default):

```
                    ┌───────────────────┐
                    │     top face      │
                    │  ░░░░░░░░░░░░░░░  │
      ┌─────────────┼───────────────────┼─────────────┬─────────────┐
      │  left face  │    front face     │ right face  │  back face  │
      │ ░░░░░░░░░░░ │ ░░░░░░░░░░░░░░░░░ │ ░░░░░░░░░░░ │ ░░░░░░░░░░░ │
      └─────────────┼───────────────────┼─────────────┴─────────────┘
                    │    bottom face    │
                    │  ░░░░░░░░░░░░░░░  │
                    └───────────────────┘
             each ░ is one patch;  n = total number of patches
```

The surface is split into `n` small, roughly square patches. Light leaving
patch *i* lands on patch *j* according to a form factor `F(i,j)`; the
radiosities `x` satisfy the linear system

```
        (I - R·F) x = R·e          R = reflectivity, e = emissivity
```

which the benchmark solves once for each colour channel.

### The pipeline that is timed

The benchmark is a sequence of seven phases, and **all seven are timed** —
input, problem setup, solution and output:

```
 geom file          ┌──────────────────────────────────────────────────┐
 (box, rho,   ────▶ │ Reader   read box size, reflectivities, emiss.   │
  emiss)            └───────────────────┬──────────────────────────────┘
                                        ▼
                    ┌──────────────────────────────────────────────────┐
                    │ Region   cut the 6 faces into n patches          │
                    └───────────────────┬──────────────────────────────┘
                                        ▼
                    ┌──────────────────────────────────────────────────┐
                    │ SetUp1   form factors for parallel faces         │
                    │ SetUp2   form factors for orthogonal faces       │
                    │ SetUp3   normalise rows, fold in rho and emiss.  │
                    └───────────────────┬──────────────────────────────┘
                                        ▼
                    ┌──────────────────────────────────────────────────┐
                    │ Solver   LDL^T factorisation + backsolve (RGB)   │
                    └───────────────────┬──────────────────────────────┘
                                        ▼
                    ┌──────────────────────────────────────────────────┐
                    │ Storer   write patch geometry + radiosities      │
                    └───────────────────┬──────────────────────────────┘
                                        ▼
                          clock stops here ──▶ Verify (untimed)
```

The clock is a wall-clock timer (`gettimeofday`, microsecond resolution).
`Meter()` takes a timestamp before `Reader` and another after `Storer`, so
the reported `Time` is one continuous interval:

```
 When()            When()                                              When()
   │                 │                                                   │
   ▼                 ▼                                                   ▼
   ├─ Reader ─ Region ─ SetUp1 ─ SetUp2 ─ SetUp3 ─ Solver ─ Storer ─┤
   │                                                                  │
   └──────────── Time = sec[7] - sec[0] (one interval) ───────────────┘
                         then, after the clock stops:
                         count FLOPs, print report, run Verify()
```

Each phase also gets an individual timestamp, so the report shows where the
time went:

```
 Task  Seconds       Operations        MFLOPS    % of Time
Reader   0.000              258     11.762287       0.2 %
Region   0.000              692    580.491674       0.0 %
SetUp1   0.002          3995883   2400.107125      15.8 %
SetUp2   0.004          7681520   1905.075098      38.2 %
SetUp3   0.000            47420    565.039476       0.8 %
Solver   0.004         11891520   2665.632510      42.2 %
Storer   0.000            11232     37.568120       2.8 %
TOTALS   0.011         23628525   2236.683674     100.0 %
```

### Turning seconds into a performance number

Counting floating-point operations from inside the code would perturb what
is being measured, so SLALOM does not count at run time. Instead, after the
clock has stopped, `Meter()` computes the operation count analytically from
`n` using fixed formulas:

| Phase | Work counted | Grows as |
|-------|--------------|----------|
| `Reader` | 258 | constant |
| `Region` | `154 + 8·r + n` | O(n) |
| `SetUp1` | `3 + 840·Σ pᵢ·pᵢ₊₃` | O(n²) |
| `SetUp2` | `430·(n² - Σpᵢ² - 2·Σ pᵢ·pᵢ₊₃)/2` | O(n²) |
| `SetUp3` | `72 + 9n + n² - Σpᵢ²` | O(n²) |
| `Solver` | leading term `n³` | O(n³) |
| `Storer` | `48·n` | O(n) |

Here `pᵢ` is the number of patches on face *i*. `Σpᵢ²` runs over all six
faces, while the opposite-face term `pᵢ·pᵢ₊₃` is summed for `i = 0, 1, 2`
(the three pairs of opposite faces). `r` is the total number of patch
columns summed over the faces.

The weights follow the LLNL operation-counting convention documented at the
top of `src/slalom.h` (for example `+`, `-`, `*` cost 1; `/` costs 4;
`sqrt` costs 4; `log`/`atan` cost 8). The total is `work`, and the headline
number is simply

```
        MFLOPS = work / Time / 1e6
```

so `M ops` is the weighted work and `MFLOPS` is that work divided by the
measured wall-clock time. The reported "Approximate data memory use" is
likewise an estimate, `8·n² + 120·n + 800` bytes (the `n x n` coefficient
matrix dominates).

### Why a fixed-time benchmark instead of a fixed problem

Traditional benchmarks run a fixed problem size and compare seconds. SLALOM
inverts that: it fixes the *time budget* (`-t`, 60 seconds is the canonical
value) and finds the largest problem the machine can finish in that budget.
Slow and fast machines therefore run the same amount of time, but different
problem sizes.

```
   Fixed problem                          SLALOM: fixed time
   (same n, compare seconds)              (same seconds, compare n)

   time ▲                                   n ▲
        │   *                                  │            ╭── result
        │  * *                                 │        ╭───╯
        │ * * *                                │    ╭───╯
        │* * *                                 │╭───╯
        └──────────────▶ n                     └──────────────▶ time
          run n = 1000                           run for 60 s
```

This is why the benchmark is "scalable": results can be compared either by
the problem size `n` reached, or by the `MFLOPS` rate, and the same code
spans machines from a Raspberry Pi to a large server. The header is explicit
that two machines should **not** be compared by the ratio of their execution
times, since those are all approximately the goal.

### Finding `n`: bisection on the time budget

`-l` and `-u` are not arbitrary search limits. The driver first *verifies*
that `-l` finishes faster than the goal and `-u` takes at least the goal, and
aborts if they do not bracket it. It then bisects the integer interval:

```
 lower                                         upper
  400 ───────────────────────────────────────── 4000
       verified fast                    verified slow
             │
             ├─ step 1: try n = 2200 → slower than goal → upper = 2200
             │                       [400 ─────────── 2200]
             ├─ step 2: try n = 1300 → faster than goal → lower = 1300
             │                                  [1300 ─── 2200]
             ├─ step 3: halve again ...
             ▼
       upper - lower = 1  →  final run at n = lower
```

The final `Meter(lower)` run is the one that is reported, so the published
`n` is the largest tested size that still completed within the budget. Each
candidate size performs the complete pipeline (including reading `geom` and
writing `answer`), so the search measures the real end-to-end cost, not a
cached or pre-warmed kernel. `NMAX` in `src/slalom.h` (8192) caps how large
the problem can get.

### Where the time goes as `n` grows

The two halves of the benchmark scale differently, and that is the point of
the measurement:

```
 time
   │                                          ╱  Solver       ~O(n³)
   │                                       ╱
   │                                   ╱ ╱
   │                            ╱ ╱ ──────  SetUp1/2/3 + I/O ~O(n²)
   │                     ╱ ╱ ──────
   │              ╱ ╱ ──────
   │       ╱ ╱─────
   │  ╱────
   └────────────────────────────────────────────────▶ n (patches)
        small n: setup dominates
                        large n: solver dominates
```

For small problems (and slower machines) the O(n²) matrix setup is most of
the time; for large problems the O(n³) factorisation dominates. Because both
phases are timed and reported separately, the benchmark characterises a
machine's memory/cache behaviour as well as its raw floating-point rate —
which is also why the unrolled `DDot()` option (`SLALOM_SUN4_DDOT`) can
change the Solver share measurably.

### Correctness checks

A fast but wrong answer is not a result. Two checks guard the measurement:

* `SetUp3` verifies that each row's total form factor is close to 1
  (relative tolerance `0.5e-9`). This check runs during setup, inside the
  timed region.
* `Verify`, which runs **after** the clock stops, recomputes `||Ax - b||`
  and rejects the run if the scaled residual exceeds `3·EPS`
  (`EPS = 0.5e-8`).

If either check fails the run prints an error instead of a performance
number.

## Running the Benchmark

```
$ ./slalom -t 10 -l 400 -u 4000
```

All three arguments are required:

* `-t <seconds>` — the fixed time budget. Use `60` for published measurements;
  a smaller value (as in the example) is convenient for a quick check.
* `-l <n>` — lower bound on the problem size; it must finish in **less** than
  the budget.
* `-u <n>` — upper bound on the problem size; it must take **at least** the
  budget.

The driver verifies that the two bounds bracket the goal, then bisects the
interval to find the largest `n` that fits in the time budget (described in
detail above). Both bounds must be between `6` (the number of faces) and
`NMAX` (8192, defined in `src/slalom.h`).

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
