cmake_minimum_required(VERSION 3.16)
project(BitScrape VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Debug)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set output directories for all targets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# For macOS, set the app bundle output directory
if(APPLE)
    set(CMAKE_MACOSX_BUNDLE TRUE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
endif()

# Find required packages
find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

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

# Set up precompiled header
target_precompile_headers(dht_hunter_core INTERFACE
  ${CMAKE_SOURCE_DIR}/include/dht_hunter/utility/common/common_utils.hpp
)

# Add subdirectories
add_subdirectory(src)

# Enable testing
enable_testing()

# Add tests
add_subdirectory(tests)

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

# macOS specific settings for app bundle
if(APPLE)
    # Set the app icon path
    set(APP_ICON_MACOSX "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/AppIcon.icns")

    # Export variables for use in src/CMakeLists.txt
    set(APP_ICON_PATH "${APP_ICON_MACOSX}" CACHE INTERNAL "Path to the app icon")

    # Note: Bundle properties, Info.plist configuration, and icon handling are now done in src/CMakeLists.txt
endif()

# Install targets
if(APPLE)
    install(TARGETS BitScrape
        BUNDLE DESTINATION .
    )
    # BitScrape_cli is installed in src/CMakeLists.txt
else()
    install(TARGETS BitScrape BitScrape_cli
        RUNTIME DESTINATION bin
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "BitScrape")
set(CPACK_PACKAGE_VENDOR "BitScrape Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BitTorrent DHT crawler and analyzer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BitScrape")

# Platform-specific package settings
if(APPLE)
    # macOS specific settings
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "BitScrape-${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_SYSTEM_NAME "macOS")
    set(CPACK_PACKAGE_FILE_NAME "BitScrape-${CPACK_PACKAGE_VERSION}-macOS")

    # Bundle all dependencies (macOS specific)
    set(CPACK_BUNDLE_NAME "BitScrape")
    set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")

    # Set icon and background image settings
    if(EXISTS "${APP_ICON_PATH}")
        set(CPACK_PACKAGE_ICON "${APP_ICON_PATH}")
        set(CPACK_BUNDLE_ICON "${APP_ICON_PATH}")
    endif()

    # Set background image if available
    set(DMG_BACKGROUND_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/dmg_background.png")
    if(EXISTS "${DMG_BACKGROUND_PATH}")
        set(CPACK_DMG_BACKGROUND_IMAGE "${DMG_BACKGROUND_PATH}")
    endif()

    # Additional DMG options
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
    set(CPACK_APPLE_BUNDLE_ID "${MACOSX_BUNDLE_GUI_IDENTIFIER}")

    # Add Applications directory symlink to the DMG
    set(CPACK_DMG_BACKGROUND_IMAGE_FOLDER "${CMAKE_CURRENT_BINARY_DIR}")
    set(CPACK_DMG_VOLUME_NAME "BitScrape")
elseif(WIN32)
    # Windows specific settings
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "BitScrape")
    set(CPACK_NSIS_DISPLAY_NAME "BitScrape")
    set(CPACK_NSIS_HELP_LINK "https://github.com/Jayian1890/bitscrape")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Jayian1890/bitscrape")
    set(CPACK_NSIS_CONTACT "jared@interlacedpixel.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_PACKAGE_FILE_NAME "BitScrape-${CPACK_PACKAGE_VERSION}-Windows")
else()
    # Linux specific settings
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jared Terrance <jared@interlacedpixel.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
    set(CPACK_PACKAGE_FILE_NAME "bitscrape-${CPACK_PACKAGE_VERSION}-Linux")
endif()

# Include CPack module
include(CPack)
