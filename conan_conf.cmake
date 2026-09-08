set(ENV{CMAKE_POLICY_VERSION_MINIMUM} "3.5")

#
# Created by Claude 09/07/2026
#

if(DEFINED ENV{CI})
    return()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CONAN_INSTALL_STAMP "${CMAKE_BINARY_DIR}/.conan_install_stamp")

set(RUN_CONAN FALSE)
if(NOT EXISTS "${CONAN_INSTALL_STAMP}")
    set(RUN_CONAN TRUE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conanfile.txt"
        AND "${CMAKE_CURRENT_SOURCE_DIR}/conanfile.txt"
        IS_NEWER_THAN "${CONAN_INSTALL_STAMP}")
    set(RUN_CONAN TRUE)
else()
    # The stamp only proves *a* conan install happened, not that it matches
    # the build_type CMake is configuring now. Conan's generated CMakeDeps
    # files gate include dirs/libs behind $<CONFIG:...> generator expressions,
    # so a stamp left over from a Release configure (e.g. CLion's "Debug" and
    # "Release" profiles sharing one build dir) silently produces empty
    # INCLUDES/DEFINES/LIBRARIES when CMAKE_BUILD_TYPE is actually Debug.
    file(STRINGS "${CONAN_INSTALL_STAMP}" STAMPED_BUILD_TYPE LIMIT_COUNT 1)
    if(NOT STAMPED_BUILD_TYPE STREQUAL CMAKE_BUILD_TYPE)
        set(RUN_CONAN TRUE)
    endif()
endif()

if(RUN_CONAN)
    message(STATUS "CLion: Running conan install from virtual environment...")

    set(CONAN_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/.venv/bin/conan")

    if(NOT EXISTS "${CONAN_EXECUTABLE}")
        message(FATAL_ERROR "Could not find conan executable in your .venv!")
    endif()

    if(APPLE)
        # C++20 module dependency scanning requires a real LLVM clang-scan-deps,
        # which AppleClang does not ship. CMakeLists.txt requires CMAKE_CXX_COMPILER_ID
        # to be "Clang" (e.g. Homebrew's llvm), not "AppleClang", so profile it as such.
        if(CMAKE_CXX_COMPILER_VERSION)
            string(REGEX MATCH "^[0-9]+" CLANG_MAJOR_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
        else()
            set(CLANG_MAJOR_VERSION "18")
        endif()
        # Conan's settings.yml whitelist lags upstream LLVM releases; clamp to the
        # newest version Conan recognizes so profile generation doesn't fail outright.
        if(CLANG_MAJOR_VERSION GREATER 22)
            set(CLANG_MAJOR_VERSION "22")
        endif()
        set(PROFILE_SETTINGS
                "compiler=clang
compiler.version=${CLANG_MAJOR_VERSION}
compiler.libcxx=libc++
compiler.cppstd=${CMAKE_CXX_STANDARD}"
        )
        set(PROFILE_CONF "")
        if(CMAKE_C_COMPILER AND CMAKE_CXX_COMPILER)
            set(PROFILE_CONF
                    "[conf]
tools.build:compiler_executables={\"c\":\"${CMAKE_C_COMPILER}\",\"cpp\":\"${CMAKE_CXX_COMPILER}\"}"
            )
        endif()
    else()
        if(CMAKE_CXX_COMPILER_VERSION)
            string(REGEX MATCH "^[0-9]+" GCC_MAJOR_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
        else()
            set(GCC_MAJOR_VERSION "11")
        endif()
        set(PROFILE_SETTINGS
                "compiler=gcc
compiler.version=${GCC_MAJOR_VERSION}
compiler.libcxx=libstdc++11
compiler.cppstd=${CMAKE_CXX_STANDARD}"
        )
        set(PROFILE_CONF "")
        if(CMAKE_C_COMPILER AND CMAKE_CXX_COMPILER)
            set(PROFILE_CONF
                    "[conf]
tools.build:compiler_executables={\"c\":\"${CMAKE_C_COMPILER}\",\"cpp\":\"${CMAKE_CXX_COMPILER}\"}"
            )
        endif()
    endif()

    # Extra profile layered on top of "default": overrides the compiler settings to
    # match CLion's toolchain. (No pinned [tool_requires] cmake anymore - that used
    # to pin cmake/3.19.8 for libpqxx's old CMake buildsystem back when this project
    # used Drogon; oat++ needs CMake >=3.20 to build from source, so the old pin now
    # actively breaks the build instead of fixing one. Revisit if a future dependency
    # needs an old CMake again - pin it then, not preemptively.)
    set(CONAN_OVERRIDE_PROFILE "${CMAKE_BINARY_DIR}/conan_override_profile.txt")
    file(WRITE "${CONAN_OVERRIDE_PROFILE}"
            "[settings]
${PROFILE_SETTINGS}
build_type=${CMAKE_BUILD_TYPE}

${PROFILE_CONF}
            ")

    execute_process(
            COMMAND "${CONAN_EXECUTABLE}" install "${CMAKE_CURRENT_SOURCE_DIR}"
            --output-folder=${CMAKE_BINARY_DIR}
            --profile:host=default
            --profile:host=${CONAN_OVERRIDE_PROFILE}
            --build=missing
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE CONAN_RES
    )

    if(NOT CONAN_RES EQUAL 0)
        message(FATAL_ERROR "Conan installation failed!")
    endif()

    file(WRITE "${CONAN_INSTALL_STAMP}" "${CMAKE_BUILD_TYPE}")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")