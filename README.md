SERIAL C SLALOM

This directory contains the source code for the unoptimized version of the
SLALOM benchmark for any uniprocessor system.  This the the plain-vanilla
version.  The only enhancement for speed is the function DDot() which
performs a double-precision dot-product.  At present, the only DDot()
implemented is for a Sun 4 (SPARC).  Other RISC systems' versions should be
very similar.

Michael Carter  11/28/90

Sample Results:
================
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
