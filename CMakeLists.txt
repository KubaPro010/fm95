cmake_minimum_required(VERSION 3.10)

project(FMTools LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)

file(GLOB SRC_FILES "src/*.c")

file(GLOB LIB_FILES "lib/*.c")

add_library(libfm OBJECT ${LIB_FILES})

set(LINK_LIBS "-lpulse -lpulse-simple -lm")

foreach(SRC_FILE ${SRC_FILES})
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    add_executable(${EXEC_NAME} ${SRC_FILE})
    target_compile_options(${EXEC_NAME} PRIVATE -O2 -Wall -Wextra -Werror -Wno-unused-parameter)

    target_link_libraries(${EXEC_NAME} PRIVATE libfm ${LINK_LIBS})
    
    install(TARGETS ${EXEC_NAME}
        DESTINATION /usr/bin
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_READ
                    WORLD_EXECUTE WORLD_READ)
endforeach()
