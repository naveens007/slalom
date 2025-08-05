cmake_minimum_required (VERSION 3.5)
project(slalom LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Set compiler standards if needed
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE on )

# Find required libraries
find_library(MATH_LIBRARY m)
find_library(CPUINFO_LIBRARY cpuinfo)

if(NOT MATH_LIBRARY)
  message(FATAL_ERROR "Math library not found")
endif()

if(NOT CPUINFO_LIBRARY)
  message(FATAL_ERROR "cpuinfo library not found") 
endif()

add_subdirectory(src)

