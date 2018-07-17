cmake_minimum_required(VERSION 3.10)
project(Embedded_RingBuf_CPP)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

set(COMMON_SOURCES
        examples_no_arduino/test.cpp
        )

include_directories(
        .
)

add_executable(Embedded_RingBuf_CPP
        ${COMMON_SOURCES}
        )
