cmake_minimum_required(VERSION 3.14)
project(DHT_Hunter VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to enable/disable static analysis
option(ENABLE_CLANG_TIDY "Enable static analysis with clang-tidy" OFF)

# Export compile commands for external tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Configure clang-tidy if enabled
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    message(STATUS "Static analysis with clang-tidy enabled")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
  else()
    message(WARNING "clang-tidy requested but executable not found")
  endif()
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
