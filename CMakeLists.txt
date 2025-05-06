cmake_minimum_required(VERSION 3.14)
project(BitScrape VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(fmt REQUIRED)

# JSON utility is part of the utility library

# Add compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX /wd4100 /wd4101 /wd4189)
else()
  add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align
    -Woverloaded-virtual -Wdouble-promotion -Wformat=2
    -Wno-error=unused-lambda-capture
    -Wno-error=unused-variable
    -Wno-error=unused-parameter
    -Wno-error=unused-private-field
    -Wno-error=unused-function
    -Wno-error=unused-local-typedefs
    -Wno-error=unused-but-set-variable
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

# Include the web bundler module
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/WebBundler.cmake)

# Bundle web files
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/web_bundler.py)
    message(STATUS "Bundling web files...")
    execute_process(
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/web_bundler.py
                --web-dir=${CMAKE_CURRENT_SOURCE_DIR}/web
                --output-dir=${CMAKE_CURRENT_SOURCE_DIR}/include/dht_hunter/web/bundle
                --namespace=dht_hunter
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE WEB_BUNDLE_RESULT
    )
    if(NOT WEB_BUNDLE_RESULT EQUAL 0)
        message(WARNING "Failed to bundle web files: ${WEB_BUNDLE_RESULT}")
    endif()
endif()

# Copy web directory to build directory (for development mode)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/web DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

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