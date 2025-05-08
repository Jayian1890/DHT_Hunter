# Enable testing with CTest
include(CTest)

# Find GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Add subdirectories for different test categories
add_subdirectory(network)
# Temporarily disable utils tests until they are updated
# add_subdirectory(utils)
