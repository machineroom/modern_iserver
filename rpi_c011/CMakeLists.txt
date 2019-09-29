cmake_minimum_required (VERSION 3.0)

add_library (rpi_c011 SHARED c011.c)
target_include_directories (rpi_c011 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries (rpi_c011 bcm2835)
install (TARGETS rpi_c011
         LIBRARY DESTINATION lib
        )
        
install(FILES c011.h DESTINATION include)

