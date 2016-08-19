cmake_minimum_required (VERSION 3.0)

add_executable (boottest boottest.c)
target_link_libraries (boottest tsp)

add_executable (looptest looptest.c)
target_link_libraries (looptest tsp)

add_executable (resettest resettest.c)
target_link_libraries (resettest tsp)

add_executable (analysetest analysetest.c)
target_link_libraries (analysetest tsp)

add_executable (soaktest soaktest.c)
target_link_libraries (soaktest tsp)

add_executable (test_enq test_enq.c)

