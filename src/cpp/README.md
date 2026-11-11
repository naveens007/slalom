# SLALOM C++ port

`slalom-cxx` is an opt-in C++17 port of the SLALOM radiosity benchmark. It
implements the benchmark stages (Reader, Region, SetUp1/2/3, Solver, Storer,
Verify, and Meter) using `std::vector`-backed geometry, matrix, and RGB vectors.
The solver retains the symmetric LDL factorization and uses the four-way
unrolled, strided `DDot` kernel used by the reference implementation's SUN4
variant. The C executable remains the reference implementation and the default
build is unchanged.

Build both executables with:

```sh
cmake -S . -B build -DSLALOM_ENABLE_CXX=ON
cmake --build build
```

Run the C++ port with the same required arguments as the C benchmark, for
example:

```sh
./build/src/cpp/slalom-cxx -t 10 -l 400 -u 4000
```

The C++ port writes its radiosity output to `answer`, reads `geom` from next to
the executable, and checks the normalized residual after solving. Its reported
work assessment follows the reference benchmark's Meter formula; floating-point
results can differ slightly due to compiler and evaluation-order differences.
