file(GLOB COROUTINE_SRC 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/*.cc"
)

add_library(sylar SHARED ${COROUTINE_SRC})
force_redefine_file_macro_for_sources(sylar)

target_include_directories(sylar PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/net
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

# add_subdirectory(http)