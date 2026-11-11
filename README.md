### Optional: the C++ implementation

`src/slalom.c` remains the reference C implementation. An opt-in C++17 port is
provided in `src/cpp/`; it implements the actual radiosity benchmark stages and
uses `std::vector`-backed matrices and vectors together with a four-way unrolled,
strided `DDot` kernel. Enable it with:

```
cmake -S /path/to/source -B /path/to/build_directory -DSLALOM_ENABLE_CXX=ON
cmake --build /path/to/build_directory
cmake --install /path/to/build_directory
```

This builds and installs `slalom-cxx` alongside the unchanged C reference. Run
it with the same `-t`, `-l`, and `-u` options:

```
/path/to/build_directory/install/bin/slalom-cxx -t 10 -l 400 -u 4000
```

The install step is required because `geom` is placed next to the installed
binary; the build-tree binary cannot find it unless `src/geom` is copied
alongside. The C++ implementation writes `answer` and validates the radiosity
residual; see `src/cpp/README.md` for details.
