# ---------------------------------------------------------------------------
# Java implementation (src/java/Slalom.java)
#
# Opt-in with -DSLALOM_ENABLE_JAVA=ON. It is OFF by default so a default build
# produces exactly the same binaries and benchmark numbers as before; the C
# implementation in ../slalom.c is the frozen reference.
#
# There is no CMake "Java language" support to lean on, so this is a custom
# target that runs javac. The class files and the geom file are placed in the
# same directory (build tree and install tree) because the benchmark looks
# for 'geom' next to its class files, mirroring the C binary which reads
# 'geom' from its own directory.
# ---------------------------------------------------------------------------

find_package(Java REQUIRED COMPONENTS Development)

set(SLALOM_JAVA_CLASSDIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
file(GLOB SLALOM_JAVA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.java")

add_custom_target(slalom-java ALL
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${SLALOM_JAVA_CLASSDIR}"
	COMMAND "${Java_JAVAC_EXECUTABLE}" -d "${SLALOM_JAVA_CLASSDIR}"
		${SLALOM_JAVA_SOURCES}
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/../geom"
		"${SLALOM_JAVA_CLASSDIR}/geom"
	DEPENDS ${SLALOM_JAVA_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/../geom"
	COMMENT "Compiling the Java implementation (javac) into ${SLALOM_JAVA_CLASSDIR}"
	VERBATIM
)

# Install the class files and geom together under bin/slalom-java.
install(DIRECTORY "${SLALOM_JAVA_CLASSDIR}/"
	DESTINATION bin/slalom-java
	FILES_MATCHING
	PATTERN "*.class"
	PATTERN "geom"
)
