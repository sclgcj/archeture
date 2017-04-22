
project(${PROJNAME})

AUX_SOURCE_DIRECTORY(. DIR_SRCS)

include_directories(${PROJSRCDIR}/tools/include)

add_subdirectory(project_manager)
