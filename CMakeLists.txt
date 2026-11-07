cmake_minimum_required(VERSION 3.16)
project(sketchlang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(sketch_core
  src/lexer.cpp src/parser.cpp src/printer.cpp
  src/bytecode.cpp src/compiler.cpp src/vm.cpp src/natives.cpp src/render.cpp)
target_include_directories(sketch_core PUBLIC src)
target_compile_options(sketch_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(sketch src/main.cpp)
target_link_libraries(sketch PRIVATE sketch_core)

option(SKETCH_BUILD_TESTS "Build unit tests" ON)
if(SKETCH_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2)
  FetchContent_MakeAvailable(Catch2)

  add_executable(tests tests/test_lexer.cpp tests/test_parser.cpp tests/test_vm.cpp)
  target_link_libraries(tests PRIVATE sketch_core Catch2::Catch2WithMain)

  include(CTest)
  include(Catch)
  catch_discover_tests(tests)
endif()
