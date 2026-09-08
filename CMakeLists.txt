cmake_minimum_required(VERSION 3.6)
project(PickUsAllBackend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "Ninja build only")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/conan_conf.cmake)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "AppleClang (no clang-scan-deps).")
endif()

find_package(oatpp REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS thread)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(PickUsAllBackend)

target_sources(PickUsAllBackend
    PUBLIC
        FILE_SET CXX_MODULES FILES
        test/test.cppm
        src/dto.cppm
)

target_sources(PickUsAllBackend
        PRIVATE
        main.cpp
        test/test.cpp
        src/dto.cpp
)



target_compile_definitions(PickUsAllBackend PRIVATE
        PICASSO_STATIC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/static")

target_link_libraries(PickUsAllBackend PRIVATE Boost::thread oatpp::oatpp Threads::Threads)
