add_executable(server main.cpp hashtable.cpp)
target_include_directories(server PUBLIC 
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/libs
)