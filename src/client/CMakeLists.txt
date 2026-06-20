add_executable(client client.cpp)
target_include_directories(client PUBLIC 
    ${CMAKE_SOURCE_DIR}/include/libs
)