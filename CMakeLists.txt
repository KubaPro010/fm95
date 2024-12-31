# Set the minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Define the project name and language
project(FMTools LANGUAGES C)

# Set the C standard (you can adjust this based on your project needs)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)

# Find all C source files in the src/ directory
file(GLOB SRC_FILES "src/*.c")

# Find all C source files in the lib/ directory
file(GLOB LIB_FILES "lib/*.c")

# Create a library to hold all object files from lib/
add_library(libfm OBJECT ${LIB_FILES})

# Linker flags for libraries
set(LINK_LIBS "-lpulse -lpulse-simple -lm")

# Loop through each file in src and create an executable
foreach(SRC_FILE ${SRC_FILES})
    # Get the filename without the directory and extension
    get_filename_component(EXEC_NAME ${SRC_FILE} NAME_WE)

    # Create the executable from each source file
    add_executable(${EXEC_NAME} ${SRC_FILE})

    # Link the necessary libraries and object files from lib/
    target_link_libraries(${EXEC_NAME} PRIVATE libfm ${LINK_LIBS})
endforeach()
