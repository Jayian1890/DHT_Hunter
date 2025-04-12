# Find Google Test package
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # If not found, use FetchContent to download and build it
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)

    # Get the source directory path where GoogleTest was downloaded
    FetchContent_GetProperties(googletest SOURCE_DIR GOOGLETEST_SOURCE_DIR)

    # Add a custom command to remove Git-related files after download
    if(EXISTS "${GOOGLETEST_SOURCE_DIR}/.git")
        message(STATUS "Removing Git-related files from GoogleTest source directory")
        file(REMOVE_RECURSE
            "${GOOGLETEST_SOURCE_DIR}/.git"
            "${GOOGLETEST_SOURCE_DIR}/.github"
            "${GOOGLETEST_SOURCE_DIR}/.gitignore"
        )
    endif()
endif()

# Add unit tests
add_subdirectory(unit)

# Add integration tests
add_subdirectory(integration)
