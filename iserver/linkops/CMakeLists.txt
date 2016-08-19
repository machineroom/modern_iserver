cmake_minimum_required (VERSION 3.0)

add_library (linkops conlib.c debug.c fix.c linkops.c lmethod.c lops.c metlnkio.c tcplink.c tops.c)
target_include_directories (linkops PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../tsp/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/../linkios
                           )
target_link_libraries (linkops linkios)                           
target_compile_definitions (linkops PRIVATE LINUX LNKtsp MES)
