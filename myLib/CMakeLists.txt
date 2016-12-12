cmake_minimum_required(VERSION 3.6)

project(MyLib VERSION 1.0.0)
include(CheckIncludeFile)

add_subdirectory(spdlog)

set(SOURCE_FILES
    mylib.cpp
    Callstack.cpp
    )

add_library(mylib SHARED ${SOURCE_FILES})

check_include_file("cxxabi.h" HAVE_CXXABI_H)
if(NOT HAVE_CXXABI_H)
    message(STATUS "CXX ABI is not available")
else()
    target_compile_definitions(mylib PUBLIC "-DHAVE_CXXABI_H")
endif()

target_compile_definitions(mylib PUBLIC "-DANDROID -DHAVE_CXXABI_H")

if (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]") #Case insensitive match
    target_compile_options(mylib PUBLIC "-fno-omit-frame-pointer")
else ()
    target_compile_options(mylib PUBLIC "-funwind-tables")
endif ()

target_link_libraries(mylib log android spdlog)
