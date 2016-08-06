cmake_minimum_required (VERSION 3.0)

add_library (tsp tsplib.c)
target_include_directories (tsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
