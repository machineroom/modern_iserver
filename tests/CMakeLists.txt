cmake_minimum_required (VERSION 3.0)

add_executable (boottest boottest.c)
target_link_libraries (boottest tsp)

add_executable (looptest looptest.c)
target_link_libraries (looptest tsp)
