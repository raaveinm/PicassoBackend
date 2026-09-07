cmake_minimum_required(VERSION 4.3)
project(PickUsAllBackend)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

add_executable(PickUsAllBackend main.cpp)
