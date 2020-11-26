cmake_minimum_required(VERSION 3.10)

project(v8qjs)

if (APPLE AND NOT IOS)
   set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
endif()

set(qjs_cflags -Wall)

set (CMAKE_CXX_STANDARD 14)

include_directories(
    ../include
)

add_library(v8qjs STATIC
        ../src/v8-impl.cc
        )

target_compile_options(v8qjs PRIVATE ${qjs_cflags})
target_compile_definitions(v8qjs PRIVATE
        _GNU_SOURCE
        CONFIG_BIGNUM
        )



