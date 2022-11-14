cmake_minimum_required (VERSION 3.5)

# Set the project name and version number. This allows for a user of your
# library or tool to specify a particular version when they include it, as in 
#   find_package(slalom 2.0 REQUIRED)

project(slalom VERSION 2.0)
set(slalom_version 2.0)

# Set the global package-wide C++ standard. This will be inherited by all
# targets specified in the project. One can also specify the C++ standard in a
# target-specific manner, using:
#   set_target_properties(foo PROPERTIES CXX_STANDARD 11)
# for a target foo
set(CMAKE_CXX_STANDARD 11)

# After a normal build, we can specify the location of various outputs of the
# build. We put executables and static libraries outside the build directory in
# bin/ and lib/, respectively.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
# Not recommended to set here. Set CC or CXX environment variables.
#set(CMAKE_C_COMPILER "gcc")
#set(CMAKE_CXX_COMPILER "g++")

SET(COMPILE_FLAGS "-O2 -DSUN4 -Wno-unused-result")
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${COMPILE_FLAGS}")
# Enable Verbose.
set(CMAKE_VERBOSE_MAKEFILE on )

# Modify below in case you want to change link time options.

#SET(GCC_COVERAGE_LINK_FLAGS    "-lgcov")
#SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

# Find cpuinfo on the system.
#find_package(PkgConfig)
#pkg_check_modules(CpuInfo REQUIRED IMPORTED_TARGET libcpuinfo0)

# Include source code and headers. This runs the CMakeLists.txt in each
# subdirectory. These can define their own libraries, executables, etc. as targets, 
# but here we define all exportable targets in the root CMakeLists.txt.
add_subdirectory(src)



#install instructions.
install(FILES src/geom DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bin")
