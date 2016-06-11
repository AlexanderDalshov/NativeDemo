project(MyLib)

set(SOURCE_FILES
    mylib.cpp
    )
add_library(mylib SHARED ${SOURCE_FILES})
target_link_libraries(mylib log android)