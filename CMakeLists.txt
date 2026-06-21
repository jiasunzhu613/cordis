cmake_minimum_required(VERSION 3.15...4.1)
project(Cordis VERSION 0.1)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include dependencies through FetchContent CMake module
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# Add subdirectories
add_subdirectory(libs/common)
add_subdirectory(src/server)
add_subdirectory(src/client)

# Link libraries to executable targets
target_link_libraries(server PUBLIC common)
target_link_libraries(client PUBLIC common)

# For GTests
enable_testing()

add_executable(hashtable_test tests/hashtable_test.cpp)
target_include_directories(hashtable_test PUBLIC ${CMAKE_SOURCE_DIR}/include/server)
target_link_libraries(hashtable_test 
    hashtable
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hashtable_test)
