
cmake_minimum_required(VERSION 3.0)

set(PROJNAME "archeture" CACHE INTERNAL "Project Name")

project(${PROJNAME})

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/include/core)
include_directories(${PROJECT_SOURCE_DIR}/include/toml ${PROJECT_SOURCE_DIR}/include/net)

#set(PROJSRCDIR, "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "Project Source Dir")
set_property(GLOBAL PROPERTY PROJSRCDIR "${PROJECT_SOURCE_DIR}")

link_directories(${PROJECT_SOURCE_DIR}/lib /usr/local/lib /usr/lib)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_definitions(-fPIC)

add_subdirectory(common)
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(example)


