cmake_minimum_required(VERSION 3.11)
project(mccomp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(SOURCES
    src/mccomp.cpp
)
set(HEADERS
    src/mccomp.h
)

add_library(mccomp STATIC ${SOURCES} ${HEADERS})

# Create an alias for the library with a namespace
add_library(mccomp::mccomp ALIAS mccomp)

target_include_directories(mccomp
    PRIVATE
        # where the library itself will look for its internal headers
        "src"
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Set additional properties for the library
set_target_properties(mccomp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${HEADERS}")

# Only build tests if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(mccomp_test "main.cpp")  
    target_link_libraries(mccomp_test ionic)
endif()
