cmake_minimum_required(VERSION 3.10)

project(FMTools LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SRC_FILES "src/*.c")

# file(GLOB LIB_FILES "lib/*.c")
file(GLOB FILTER_FILES "filter/*.c")
file(GLOB MODULATION_FILES "modulation/*.c")
file(GLOB DSP_FILES "dsp/*.c")
file(GLOB IO_FILES "io/*.c")

file(GLOB INIH_FILES "inih/*.c")

add_library(inih OBJECT ${INIH_FILES})

# add_library(libfm OBJECT ${LIB_FILES})
add_library(libfmfilter OBJECT ${FILTER_FILES})
add_library(libfmmodulation OBJECT ${MODULATION_FILES})
add_library(libfmdsp OBJECT ${DSP_FILES})
add_library(libfmio OBJECT ${IO_FILES})


# Define DEBUG macro for Debug builds on libraries
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # target_compile_definitions(libfm PRIVATE DEBUG=1)
    target_compile_definitions(libfmfilter PRIVATE DEBUG=1)
    target_compile_definitions(libfmmodulation PRIVATE DEBUG=1)
    target_compile_definitions(libfmdsp PRIVATE DEBUG=1)
    target_compile_definitions(libfmio PRIVATE DEBUG=1)
endif()

set(FM_LIBS inih libfmfilter libfmmodulation libfmio libfmdsp pulse pulse-simple m liquid)

foreach(SRC_FILE ${SRC_FILES})
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    add_executable(${EXEC_NAME} ${SRC_FILE})
    target_compile_options(${EXEC_NAME} PRIVATE -O2 -Wall -Wextra -Werror -Wno-unused-parameter)
    target_link_libraries(${EXEC_NAME} PRIVATE ${FM_LIBS})
    
    # Define DEBUG macro for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${EXEC_NAME} PRIVATE DEBUG=1)
    endif()

    install(TARGETS ${EXEC_NAME}
        DESTINATION /usr/bin
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                    GROUP_EXECUTE GROUP_WRITE GROUP_READ
                    WORLD_EXECUTE WORLD_READ)
endforeach()