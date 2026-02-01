cmake_minimum_required(VERSION 3.11)
project(mccomp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(SRC 
    src/mccomp.cpp
    src/mccomp.h
    main.cpp
)

add_executable(mccomp ${SRC})  

set_target_properties(mccomp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "..")

