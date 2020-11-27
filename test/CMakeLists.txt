# Tencent is pleased to support the open source community by making xLua available.
# Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.
# Licensed under the MIT License (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
# http://opensource.org/licenses/MIT
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

cmake_minimum_required(VERSION 2.8)

if ( WIN32 AND NOT CYGWIN AND NOT ( CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" ) AND NOT ANDROID)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT" CACHE STRING "")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd" CACHE STRING "")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT" CACHE STRING "")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd" CACHE STRING "")
endif ()

#if ( WIN32)
#    add_compile_options("/std:c++latest")
#endif ()

project(Helloworld)

set (CMAKE_CXX_STANDARD 14)

if ( IOS )
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fembed-bitcode")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fembed-bitcode")
endif ()


find_path(PUERTS_PROJECT_DIR NAMES SConstruct
    PATHS 
    ${CMAKE_SOURCE_DIR}
    NO_DEFAULT_PATH
    )

MARK_AS_ADVANCED(PUERTS_PROJECT_DIR)

set(JS_ENGINE_ROOT ${CMAKE_SOURCE_DIR}/${JS_ENGINE})

include_directories(
    ${JS_ENGINE_ROOT}/Inc
)

set ( SRC hello-world.cc)

add_executable(helloworld ${SRC})

#if (WIN32)
#set_property(TARGET helloworld PROPERTY CXX_STANDARD 20)
#set_property(TARGET helloworld PROPERTY CXX_STANDARD_REQUIRED ON)
#else ()
#endif ()



if ( ANDROID )
    find_library(log-lib log )

    if( ANDROID_ABI STREQUAL "armeabi-v7a")
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Android/armv7/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM)
    else ( )
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Android/arm64/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM64)
    endif ( )
elseif ( APPLE )
    if (IOS)
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/iOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_IOS)
    else ()
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/macOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_MAC)
    endif ( )
else ()
    if ( JS_ENGINE STREQUAL "v8")
        set(LIBS_NEEDED winmm.lib dbghelp.lib shlwapi.lib)
    endif()
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        target_link_libraries(hello-world
            ${JS_ENGINE_ROOT}/Lib/Win32/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    else ()
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Win64/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    endif ()
    target_compile_definitions (helloworld PRIVATE PLATFORM_WINDOWS)
endif ( )
