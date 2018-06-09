SERIAL C SLALOM

This directory contains the source code for the unoptimized version of the
SLALOM benchmark for any uniprocessor system.  This the the plain-vanilla
version.  The only enhancement for speed is the function DDot() which
performs a double-precision dot-product.  At present, the only DDot()
implemented is for a Sun 4 (SPARC).  Other RISC systems' versions should be
very similar.

Michael Carter  11/28/90
```
/******************************************************************************
                               S L A L O M
  Scalable Language-independent Ames Laboratory One-minute Measurement
  The following program is the first benchmark based on fixed time rather
  than fixed problem comparison.  Not only is fixed time more representative
  of the way people use computers, it also greatly increases the scope and
  longevity of the benchmark.  SLALOM is very scalable, and can be used to
  compare computers as slow as 152 floating-point operations per second
  (FLOPS) to computers running a trillion times faster.  The scalability can
  be used to compare single processors to massively parallel collections
  of processors, and to study the space of problem size vs. ensemble size
  in fine detail.  It resembles the LINPACK benchmark since it involves
  factoring and backsolving a (nearly) dense 64-bit matrix, but incorporates
  a number of improvements to that benchmark that we hope will make SLALOM
  a better reflection of general system performance.
     The SLALOM benchmark solves a complete, real problem (optical radiosity
  on the interior of a box), not a contrived kernel or a synthetic mixture of
  sample operations.  SLALOM is unusual since it times input, problem setup,
  solution, and output, not just the solution.  For slower computers, the
  problem setup will take the majority of the time; it grows as the square of
  the problem size.  The solver grows as the cube of the problem size, and
  dominates the time for large values of n.
     While the following is C, you are free to translate it into any
  language you like, including assembly language specific to one computer.
  You may use compiler directives, hand-tuned library calls, loop unrolling,
  and even change the algorithm, if you can provide a convincing argument
  that the program still works for the full range of possible inputs.  For
  example, if you replace the direct solver with an iterative one, you must
  make sure your method is correct even when the geometry is quite eccentric
  and the box faces are highly reflective. (rho = .999)
     The main() driver should be used with the value of 60 seconds for the
  SLALOM benchmark.  The work done for a particular problem size is figured
  after timing has ceased, so there is no overhead for work assessment.  The
  residual check ||Ax - b|| is also done after timing has ceased.  Two
  computers may be compared either by their problem size n, or by their MFLOPS
  rate, never by the ratio of execution times.  Times will always be near one
  minute in SLALOM.  We have used the following weights for floating-point
  operation counting, based on the weights used by Lawrence Livermore National
  Laboratory:
                        OPERATION                       WEIGHT
                    a=b, a=(constant)                      0
       a<0, a<=0, a==0, a!=0, a>0, a>=0                    0
                 -a, fabs(a), fsgn(a, b)                   0
                   a+b, a-b, a*b, a^2                      1
       a<b, a<=b, a==b, a!=b, a>b, a>=b                    1
                (int) a, (double)b                         1
                        1/a, -1/a                          3
                           a/b                             4
                          sqrt(a)                          4
               Format to or from ASCII string              6
       sin(a), cos(a), tan(a), log(a), atan(a), exp(a)     8
     We invite you to share with us the results of any measurements that you
  make with SLALOM.  We do NOT accept anonymous data; machine timings will be
  referenced and dated.  For any one machine, we are interested in knowing
  two extremes: performance with as little alteration as possible from the
  following specification or similar high-level language, and the performance
  with every tuning you can think of.  The former rewards clever optimizing
  compilers, while the latter allows advanced architectures to show what they
  can achieve.
     The least you need to do to adapt SLALOM to your computer is:
        1.  In the "Measure" routine, set NMAX to a value large enough to keep
            the computer working for a minute.  Vary it slightly if it helps
            (for reasons of cache size, interleaving, etc.)
        2.  Replace the timer call in "When" with the most accurate wall-clock
            timer at your disposal.  If only CPU time is available, try to run
            the job standalone or at high priority, since we are ultimately
            interested in the top of the statistical range of performance.
        3.  Edit in the information specific to your test in the "What"
            routine, so that final output will be automatically annotated.
        4.  Compile, link, and run the program, interacting to select values
            of n that bracket a time of one minute.  Once everything is
            running, run it as a batch job so as to record the session.
     Examples of ways you may optimize performance:
        1.  Unroll the loops in SetUp1 and SetUp2; it is possible to
            vectorize both SetUp1 and SetUp2 at the cost of some extra
            operations, program complexity, and storage.
        2.  Replace the innermost loops of Solver with calls to well-tuned
            libraries of linear algebra routines, such as DDOT from the
            Basic Linear Algebra Subroutines (level 1 BLAS).  Better still,
            use a tuned library routine for all of Solver; the sparsity
            exploited in Solver is only a few percent, so you will usually
            gain more than you lose by applying a dense symmetric solver.
        3.  Parallelize the SetUp and Solver routines; all are highly
            parallel.  Each element of the matrix can be constructed
            independently, once each processor knows the geometry and part of
            the partitioning into regions.  A substantial body of literature
            now exists for performing the types of operations in Solver in
            parallel.
        4.  Overlap computation with output.  Once the Region routine is done,
            the first part of the output file (patch geometry) can be written
            while the radiosities are being calculated.
     Examples of what you may NOT do:
        1.  The tuning must not be made specific to the particular input
            provided.  For example, you may not eliminate IF tests simply
            because they always come out the same way for this input; you
            may not use precomputed answers or table look-up unless those
            answers and tables cover the full range of possible inputs; and
            you may not exploit symmetry for even values of the problem size.
        2.  You may not disable the self-consistency tests in SetUp3 and
            Verify, nor alter their tolerance constants.
        3.  You may not change the input or output files to unformatted
            binary or other format that would render them difficult to create
            or read for humans.
        4.  You may not eliminate the reading of the "geom" file by putting
            its data directly into the compiled program.
        5.  You may not change any of the work assessments in Meter.  If you
            use more floating-point operations than indicated, you must still
            use the assessments provided.  If you find a way to use fewer
            operations and still get the job done for arbitrary input
            parameters, please tell us!
                          -John Gustafson, Diane Rover, Michael Carter,
                           and Stephen Elbert
                           Ames Laboratory, Ames, Iowa 50011
******************************************************************************/
```

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

Machine:  64 Bit       Processor:  x86-64
Memory:   2 GB                 # of procs: 1
Cache:                         # used:     1
NMAX:     4096                 Clock:      3 GHz
Disk:     20 GB                Node name:  centos
OS:       Centos 7             Timer:      Wall, gettimeofday()
Language: C                    Alone:      yes
Compiler: gcc                  Run by:     Naveen Sharma
Options:  -O3                  Date:       03 May 2018
M ops:    44824.4              Time:       29.942 seconds
n:        3606                 MFLOPS:     1497.1

----------------------------------------------------------------------
```
