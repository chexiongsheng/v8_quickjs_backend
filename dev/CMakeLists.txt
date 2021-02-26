cmake_minimum_required(VERSION 3.10)

project(Helloworld)

if (APPLE AND NOT IOS)
   set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
endif()

set(qjs_cflags -Wall)
if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang")
    list(APPEND qjs_cflags
            -Wextra
            -Wno-sign-compare
            -Wno-missing-field-initializers
            -Wno-unused-parameter
            -Wno-unused-variable
            -funsigned-char)
else()
    list(APPEND qjs_cflags
            -Wno-array-bounds
            -Wno-unused-variable
            -Wno-unused-but-set-variable)
endif()

set(SRC_FILES
        include/v8.h
        quickjs/cutils.c
        quickjs/libbf.c
        quickjs/libregexp.c
        quickjs/libunicode.c
        quickjs/quickjs.c
        src/v8-impl.cc
        )
        
file(STRINGS "quickjs/VERSION" QJS_VERSION_STR)

set (CMAKE_CXX_STANDARD 14)

include_directories(
    include
    quickjs
)

add_executable(helloworld ${SRC_FILES} test/hello-world.cc)

if ( MSYS )
target_compile_definitions(helloworld PRIVATE
        _GNU_SOURCE
        CONFIG_BIGNUM
        CONFIG_VERSION="\\\"${QJS_VERSION_STR}\\\""
        )
else()
target_compile_definitions(helloworld PRIVATE
        _GNU_SOURCE
        CONFIG_BIGNUM
        CONFIG_VERSION="${QJS_VERSION_STR}"
        )
endif()

#set_target_properties(helloworld PROPERTIES
#        C_STANDARD 99
#        C_STANDARD_REQUIRED ON
#        )
target_compile_options(helloworld PRIVATE ${qjs_cflags})
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(helloworld PRIVATE
            DUMP_LEAKS
            )
endif()

target_include_directories(helloworld PUBLIC ${CMAKE_SOURCE_DIR})
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_libraries(helloworld atomic)
endif()


if ( APPLE )
    target_compile_definitions (helloworld PRIVATE PLATFORM_MAC)
else ()
    target_compile_definitions (helloworld PRIVATE PLATFORM_WINDOWS)
endif ( )
