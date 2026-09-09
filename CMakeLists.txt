cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0111 OLD)

project(PickUsAllBackend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/conan_conf.cmake)

find_package(oatpp REQUIRED)
find_package(oatpp-websocket REQUIRED)
find_package(oatpp-postgresql REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS thread)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(src)

add_executable(PickUsAllBackend main.cpp)

target_link_libraries(PickUsAllBackend
        PRIVATE picasso_core Boost::thread Threads::Threads)
