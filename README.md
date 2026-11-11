### Optional: the C++ implementation

`src/slalom.c` remains the reference C implementation. An opt-in C++17 port is
provided in `src/cpp/`; it implements the actual radiosity benchmark stages and
uses `std::vector`-backed matrices and vectors together with a four-way unrolled,
strided `DDot` kernel. Enable it with:

```
cmake -S /path/to/source -B /path/to/build_directory -DSLALOM_ENABLE_CXX=ON
cmake --build /path/to/build_directory
```

This builds `slalom-cxx` alongside the unchanged C reference. Run it with the
same `-t`, `-l`, and `-u` options. The C++ implementation writes `answer` and
validates the radiosity residual; see `src/cpp/README.md` for details.
