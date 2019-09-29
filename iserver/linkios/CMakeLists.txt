cmake_minimum_required (VERSION 3.0)

add_library (linkios tsplink.c c011link.c)

target_include_directories (linkios PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../tsp/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../c011
                            ${CMAKE_CURRENT_SOURCE_DIR}/../linkops
                           )
target_link_libraries (linkios tsp rpi_c011)
                           
target_compile_definitions (linkios PRIVATE LINUX ) 
