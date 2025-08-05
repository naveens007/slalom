include_directories(./ src)

SET(SOURCES sl-driver.c slalom.c system_info.c)
SET(HEADER_FILES slalom.h system_info.h)

add_executable(slalom ${SOURCES} ${HEADER_FILES})

# Include directories (current directory)
target_include_directories(slalom PRIVATE .)

# Link libraries
target_link_libraries(slalom
	${MATH_LIBRARY}
	${CPUINFO_LIBRARY}
)

# Installation
install(TARGETS slalom
	DESTINATION bin
)

# Install the geom file (read-only data)
install(FILES geom
        DESTINATION bin
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
)
