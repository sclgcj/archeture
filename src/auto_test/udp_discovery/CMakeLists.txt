project(${PROJNAME})
AUX_SOURCE_DIRECTORY(. DIR_SRCS)
add_library(udp_discovery SHARED ${DIR_SRCS})
target_link_libraries(udp_discovery net core)
