cmake_minimum_required(VERSION 3.16)
project(KuraCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE KURA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
add_executable(KuraCpp WIN32 ${KURA_SOURCES})
target_link_libraries(KuraCpp PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(KuraCpp PRIVATE /utf-8)
endif()
