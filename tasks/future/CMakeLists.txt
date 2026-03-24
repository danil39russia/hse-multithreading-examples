cmake_minimum_required(VERSION 3.25)

project(future_hw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(process_pool_demo process_pool_demo.cpp)
target_link_libraries(process_pool_demo PRIVATE Threads::Threads)
