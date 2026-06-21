add_executable(server main.cpp hashtable.cpp)
target_include_directories(server PUBLIC 
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/libs
)

# TODO: not sure what the best way to link gtest to the actual code is
add_library(hashtable hashtable.cpp)
target_include_directories(hashtable PUBLIC 
    ${CMAKE_SOURCE_DIR}/include/server
    ${CMAKE_SOURCE_DIR}/include/libs
)