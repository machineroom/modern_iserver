cmake_minimum_required (VERSION 3.0)

add_executable (iserver boot.c files.c info.c misc.c pack.c record.c serverc.c ttyio.c
                filec.c hostc.c iserver.c msdosc.c parsecl.c serve.c sh.c vaxrms.c)

target_link_libraries (iserver tsp linkios linkops)

target_include_directories (iserver PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../linkops
                           )
                           
target_compile_definitions (iserver PRIVATE LINUX PROTOTYPES)
