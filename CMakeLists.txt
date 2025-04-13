cmake_minimum_required(VERSION 3.14)
project(DHT_Hunter VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wdouble-promotion -Wformat=2
  )
endif()

# Include directories
include_directories(include)

# Add subdirectories
add_subdirectory(src)

# Enable testing
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Build examples
option(BUILD_EXAMPLES "Build the examples" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
