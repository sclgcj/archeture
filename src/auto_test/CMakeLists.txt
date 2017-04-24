project(${PROJNAME})
AUX_SOURCE_DIRECTORY(. DIR_SRCS)

add_subdirectory(udp_discovery)
add_subdirectory(main)
