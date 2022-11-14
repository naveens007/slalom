
include_directories(./ src)
SET(SOURCES sl-driver.c slalom.c system_info.c)
SET(HEADER_FILES slalom.h system_info.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADER_FILES})
#add_executable(${PROJECT_NAME} system_info.c system_info.h)

# Specify the library dependencies.
target_link_libraries(${PROJECT_NAME} PRIVATE m)
#target_link_libraries(${PROJECT_NAME} PkgConfig::CpuInfo)
target_link_libraries(${PROJECT_NAME} PRIVATE cpuinfo)
