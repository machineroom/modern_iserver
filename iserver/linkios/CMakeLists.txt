cmake_minimum_required (VERSION 3.0)

add_library (linkios tsplink.c)

target_include_directories (linkios PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../tsp/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/../linkops
                           )
                           
target_compile_definitions (linkios PRIVATE LINUX PROTOTYPES) 
