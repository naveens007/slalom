SERIAL C SLALOM

This directory contains the source code for the unoptimized version of the
SLALOM benchmark for any uniprocessor system.  This the the plain-vanilla
version.  The only enhancement for speed is the function DDot() which
performs a double-precision dot-product.  At present, the only DDot()
implemented is for a Sun 4 (SPARC).  Other RISC systems' versions should be
very similar.

Michael Carter  11/28/90
