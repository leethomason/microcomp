cmake_minimum_required(VERSION 3.11)
project(mccomp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Library source files
add_library(mccomp STATIC
    src/mccomp.cpp
    src/mccomp.h
)

# Create an alias with namespace for consistent usage
add_library(mccomp::mccomp ALIAS mccomp)

# Configure include directories for FetchContent compatibility
target_include_directories(mccomp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Set library properties
set_target_properties(mccomp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PUBLIC_HEADER "src/mccomp.h"
)

# Only build tests when this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(mccomp_test main.cpp)
    target_link_libraries(mccomp_test mccomp::mccomp)
    set_target_properties(mccomp_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "..")
endif()
