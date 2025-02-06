cmake_minimum_required (VERSION 3.20)

project(pigeon CXX)
set (CMAKE_CXX_STANDARD 11)

Include(FetchContent)

set(CMAKE_BUILD_TYPE "Debug")

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_library(pigeon INTERFACE)
add_library(pigeon::pigeon ALIAS pigeon)
target_include_directories(pigeon INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(tests)
add_subdirectory(tutorial)
add_subdirectory(examples)
