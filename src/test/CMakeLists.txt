project(${PROJNAME})
AUX_SOURCE_DIRECTORY(. DIR_SRCS)
add_executable(auto_test_test ${DIR_SRCS})
target_link_libraries(auto_test_test core)