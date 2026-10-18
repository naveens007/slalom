include_directories(./ src)

SET(SOURCES sl-driver.c slalom.c system_info.c)
SET(HEADER_FILES slalom.h system_info.h)

add_executable(slalom ${SOURCES} ${HEADER_FILES})

# Include directories (current directory)
target_include_directories(slalom PRIVATE .)

# Report the real build configuration in the benchmark banner instead of the
# hard-coded "-O2 -DSUN4" string that used to be printed unconditionally.
target_compile_definitions(slalom PRIVATE
	SLALOM_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

# Select the unrolled dot product when requested.
if(SLALOM_SUN4_DDOT)
	target_compile_definitions(slalom PRIVATE SUN4)
endif()

# Link libraries
target_link_libraries(slalom
	${MATH_LIBRARY}
)

# libcpuinfo is optional (see the top-level CMakeLists.txt).
if(SLALOM_HAVE_CPUINFO)
	target_include_directories(slalom PRIVATE ${CPUINFO_INCLUDE_DIR})
	target_link_libraries(slalom ${CPUINFO_LIBRARY})
	target_compile_definitions(slalom PRIVATE SLALOM_HAVE_CPUINFO=1)
endif()

# Installation
install(TARGETS slalom
	DESTINATION bin
)

# Install the geom file (read-only data)
install(FILES geom
        DESTINATION bin
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
)

# Experimental C++ implementation. Opt-in; see SLALOM_ENABLE_CXX in the
# top-level CMakeLists.txt.
if(SLALOM_ENABLE_CXX)
	add_subdirectory(cpp)
endif()
