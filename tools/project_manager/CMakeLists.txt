
project(${PROJNAME})

AUX_SOURCE_DIRECTORY(. DIR_SRCS)

add_executable(project_manager ${DIR_SRCS})
