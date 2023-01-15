cmake_minimum_required (VERSION 3.10)
project(lzpico)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# options

option(LZPICO_ALL_WARNINGS            "lzpico: enable all compiler warnings" ON)
option(LZPICO_ALL_WARNINGS_3RD_PARTY  "lzpico: enable all compiler warnings in 3rd party libs" OFF)

option(LZPICO_SANITIZE_THREAD         "lzpico: enable thread sanitizer" OFF)
option(LZPICO_SANITIZE_ADDRESS        "lzpico: enable address sanitizer" OFF)
option(LZPICO_SANITIZE_UNDEFINED      "lzpico: enable undefined sanitizer" OFF)

# sanitizers

if (LZPICO_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (LZPICO_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

if (LZPICO_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# main

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif ()

add_subdirectory(third-party)
add_subdirectory(src)
