cmake_minimum_required(VERSION 3.4)

project(MyLib)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(spdlog)

set(SOURCE_FILES
    mylib.cpp
    )

add_library(mylib SHARED ${SOURCE_FILES})
target_link_libraries(mylib log android spdlog)

