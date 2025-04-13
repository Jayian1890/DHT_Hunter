# Find Google Test package
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # If not found, use FetchContent to download and build it
    include(FetchContent)

    # Use URL instead of GIT_REPOSITORY to avoid Git-related issues
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        URL_HASH SHA256=24564e3b712d3eb30ac9a85d92f7d720f60cc0173730ac166f27dda7fed76cb2
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # Disable tests in GoogleTest itself
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
endif()

# Add unit tests
add_subdirectory(unit)

# Add integration tests
add_subdirectory(integration)
