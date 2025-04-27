cmake_minimum_required(VERSION 3.14)
project(DHT_Hunter VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(fmt REQUIRED)

# Add compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align
    -Woverloaded-virtual -Wdouble-promotion -Wformat=2
    -Wno-error=unused-lambda-capture
  )
endif()

# Enable AddressSanitizer for memory debugging
option(ENABLE_ASAN "Enable AddressSanitizer for memory debugging" OFF)
if(ENABLE_ASAN)
  if(APPLE OR UNIX)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
endif()

# Fix FetchContent timestamp warning
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# Create a core library target for common include directories
add_library(dht_hunter_core INTERFACE)
target_include_directories(dht_hunter_core INTERFACE
  ${CMAKE_SOURCE_DIR}/include
)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)

# Tests have been removed from the project
# TODO: Consider implementing a new testing framework in the future if needed

# Install targets
install(TARGETS dht_hunter
    RUNTIME DESTINATION bin
)

# Add CMake presets support
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
  # Optional: Create a basic CMakePresets.json if it doesn't exist
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/CMakePresets.json")
    file(WRITE "${CMAKE_SOURCE_DIR}/CMakePresets.json"
    [[{
      "version": 3,
      "configurePresets": [
        {
          "name": "default",
          "displayName": "Default Config",
          "description": "Default build using Ninja generator",
          "generator": "Ninja",
          "binaryDir": "${sourceDir}/build"
        },
        {
          "name": "debug",
          "inherits": "default",
          "displayName": "Debug Build",
          "cacheVariables": {
            "CMAKE_BUILD_TYPE": "Debug",
            "ENABLE_ASAN": "ON"
          }
        },
        {
          "name": "release",
          "inherits": "default",
          "displayName": "Release Build",
          "cacheVariables": {
            "CMAKE_BUILD_TYPE": "Release"
          }
        }
      ]
    }]])
  endif()
endif()