# ---------------------------------------------------------------------------
# C++ implementation (work in progress)
#
# Opt-in with -DSLALOM_ENABLE_CXX=ON. It is OFF by default so a default build
# produces exactly the same binaries and benchmark numbers as before; the C
# implementation in ../slalom.c is the frozen reference.
# ---------------------------------------------------------------------------

add_executable(slalom-cxx
	slalom-cxx.cpp
	file.hpp
	# The machine report is still the shared C implementation. Compiling it
	# into this target is what verifies the mixed C/C++ linkage.
	"${CMAKE_CURRENT_SOURCE_DIR}/../system_info.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../system_info.h"
)

target_include_directories(slalom-cxx PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Report the real build configuration, matching the C target.
target_compile_definitions(slalom-cxx PRIVATE
	SLALOM_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

target_link_libraries(slalom-cxx
	${MATH_LIBRARY}
)

# libcpuinfo is optional (see the top-level CMakeLists.txt).
if(SLALOM_HAVE_CPUINFO)
	target_include_directories(slalom-cxx PRIVATE ${CPUINFO_INCLUDE_DIR})
	target_link_libraries(slalom-cxx ${CPUINFO_LIBRARY})
	target_compile_definitions(slalom-cxx PRIVATE SLALOM_HAVE_CPUINFO=1)
endif()

# geom is installed next to it by the parent CMakeLists.txt.
install(TARGETS slalom-cxx
	DESTINATION bin
)