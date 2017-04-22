
project(${PROJNAME})

AUX_SOURCE_DIRECTORY(. DIR_SRCS)

#get_property(PROJ_SOURCE_DIR GLOBAL PROPERTY "PROJSRCDIR")
#MESSAGE(STATUS "net source dir = ${PROJ_SOURCE_DIR}")
#include_directories(${PROJ_SOURCE_DIR}/include/core ${PROJ_SOURCE_DIR}/include/net)
#include_directories(${PROJSRCDIR}/include/core)
#include_directories(${PROJSRCDIR}/include/net)
add_library(net SHARED ${DIR_SRCS})
target_link_libraries(net core)
