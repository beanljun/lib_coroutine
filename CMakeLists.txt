cmake_minimum_required(VERSION 3.0)
project(sylar-from-scratch)

include(cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)

# 指定编译选项
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -ggdb -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated -Wno-deprecated-declarations")

include_directories(.)

option(BUILD_TEST "ON for compile test" ON)

find_package(Boost REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# 使用 file(GLOB ...) 来自动收集源文件
file(GLOB_RECURSE LIB_SRC 
    "sylar/*.cpp"
    "sylar/*.cc"
    "sylar/http/*.cc"
    "sylar/streams/*.cc"
    "sylar/http/http-parser/*.c"
)

add_library(sylar SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(sylar)

set(LIBS
    sylar
    pthread
    dl
    yaml-cpp
)

if(BUILD_TEST)
    file(GLOB TEST_SRC "tests/*.cpp" "tests/*.cc")
    foreach(testfile ${TEST_SRC})
        get_filename_component(testname ${testfile} NAME_WE)
        sylar_add_executable(${testname} ${testfile} sylar "${LIBS}")
    endforeach()
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)