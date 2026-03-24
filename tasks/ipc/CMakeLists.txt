cmake_minimum_required(VERSION 3.25)

project(ipc_mpsc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(ipc_producer producer_main.cpp)
target_link_libraries(ipc_producer PRIVATE Threads::Threads)

add_executable(ipc_consumer consumer_main.cpp)
target_link_libraries(ipc_consumer PRIVATE Threads::Threads)
