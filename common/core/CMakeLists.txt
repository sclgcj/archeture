
project(${PROJNAME})

AUX_SOURCE_DIRECTORY(. DIR_SRCS)

#get_property(PROJ_SOURCE_DIR GLOBAL PROPERTY "PROJSRCDIR")
#MESSAGE(STATUS "core source dir = ${PROJ_SOURCE_IR}")
#include_directories(${PROJ_SOURCE_DIR}/include/core)

add_library(core SHARED ${DIR_SRCS})

target_link_libraries(core cJSON pthread m dl rt z toml)
