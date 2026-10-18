cmake_minimum_required (VERSION 3.5)
project(slalom LANGUAGES C CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# A benchmark must be optimized; without this a bare `cmake` configure would
# produce an unoptimized (-O0) binary and meaningless results.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to 'Release'")
endif()

# Set compiler standards if needed
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE on )

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

# Use the unrolled double-precision dot product (the "SUN4" variant in
# slalom.c). Despite the SPARC-oriented comment it is portable C and speeds up
# most pipelined cores.
option(SLALOM_SUN4_DDOT "Use the unrolled (SUN4) DDot" OFF)

# Detect processor details with libcpuinfo. When unavailable the benchmark
# falls back to reading /proc/cpuinfo, so a missing library is not fatal.
option(SLALOM_USE_CPUINFO "Use libcpuinfo for processor detection" ON)

# Build the experimental C++ implementation (target: slalom-cxx).
#
# OFF by default on purpose: src/slalom.c is the frozen reference benchmark
# whose numbers are published, so the default build must keep producing
# exactly the same binaries. Enable it explicitly to build the C++ version
# alongside the C one.
option(SLALOM_ENABLE_CXX "Build the C++ implementation (target slalom-cxx)" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
  message(FATAL_ERROR "Math library not found")
endif()

set(SLALOM_HAVE_CPUINFO OFF)
if(SLALOM_USE_CPUINFO)
  # find_path is required: the header is not located from CPUINFO_LIBRARY
  # alone, which is why the build used to fail against a non-system install
  # (e.g. a prefix passed via CMAKE_PREFIX_PATH).
  find_path(CPUINFO_INCLUDE_DIR cpuinfo.h)
  find_library(CPUINFO_LIBRARY cpuinfo)
  if(CPUINFO_LIBRARY AND CPUINFO_INCLUDE_DIR)
    set(SLALOM_HAVE_CPUINFO ON)
    message(STATUS "Found cpuinfo: ${CPUINFO_LIBRARY} (headers: ${CPUINFO_INCLUDE_DIR})")
  else()
    message(WARNING
      "libcpuinfo not found (library='${CPUINFO_LIBRARY}' header='${CPUINFO_INCLUDE_DIR}'). "
      "Falling back to /proc/cpuinfo based processor detection. "
      "Install it (e.g. 'apt install libcpuinfo-dev') or pass "
      "-DCPUINFO_LIBRARY=... -DCPUINFO_INCLUDE_DIR=... for a richer report.")
  endif()
endif()

add_subdirectory(src)
