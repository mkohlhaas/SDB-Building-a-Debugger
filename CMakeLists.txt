cmake_minimum_required(VERSION 3.19)

project(sdb LANGUAGES CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libedit REQUIRED IMPORTED_TARGET libedit)
find_package(fmt CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)

include(CTest)

add_subdirectory(src)
add_subdirectory(tools)

if(BUILD_TESTING)
  find_package(Catch2 CONFIG REQUIRED)
  add_subdirectory(test)
endif()
