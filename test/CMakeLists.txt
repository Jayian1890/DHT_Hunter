cmake_minimum_required(VERSION 3.14)

# Enable testing
enable_testing()

# Disable strict warnings for tests
if(NOT MSVC)
  add_compile_options(-Wno-double-promotion)
endif()

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add test subdirectories
add_subdirectory(unit)
