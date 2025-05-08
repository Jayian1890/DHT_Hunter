# Add all components in the correct dependency order
add_subdirectory(types)     # Types component
add_subdirectory(utility)   # Utility functions
add_subdirectory(network)   # Network component
add_subdirectory(bencode)   # Bencode component
add_subdirectory(dht)       # DHT component
add_subdirectory(bittorrent) # BitTorrent component
add_subdirectory(unified_event) # Unified event system
add_subdirectory(web)       # Web UI component

# Removed components:
# - crawler (not used in the build)
# - metadata (not used in the build)
# - service (not used in the build)
# - storage (not used in the build)
# - ui (not used in the build)

# Main executable
if(APPLE)
    # Set the icon file path
    set(APP_ICON_MACOSX "${CMAKE_CURRENT_SOURCE_DIR}/../resources/macos/AppIcon.icns")

    # Create the executable with the icon file
    if(EXISTS "${APP_ICON_MACOSX}")
        # Add the icon file to the executable
        add_executable(BitScrape MACOSX_BUNDLE main.cpp "${APP_ICON_MACOSX}")

        # Set the icon file properties
        set_source_files_properties("${APP_ICON_MACOSX}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
    else()
        # Create the executable without the icon
        add_executable(BitScrape MACOSX_BUNDLE main.cpp)
    endif()

    # Set the bundle properties for the target
    set_target_properties(BitScrape PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../resources/macos/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.bitscrape.app"
        MACOSX_BUNDLE_BUNDLE_NAME "BitScrape"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025. All rights reserved."
        MACOSX_BUNDLE_INFO_STRING "BitScrape - BitTorrent DHT crawler and analyzer"
        MACOSX_BUNDLE_EXECUTABLE_NAME "BitScrape"
    )

    # Add a post-build command to ensure the Resources directory exists
    add_custom_command(TARGET BitScrape POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_BUNDLE_DIR:BitScrape>/Contents/Resources
        COMMENT "Creating Resources directory"
    )

    # Add a post-build command to copy the icon file if it exists
    if(EXISTS "${APP_ICON_MACOSX}")
        add_custom_command(TARGET BitScrape POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${APP_ICON_MACOSX}"
                $<TARGET_BUNDLE_DIR:BitScrape>/Contents/Resources/AppIcon.icns
            COMMENT "Copying app icon to bundle"
        )
    endif()
else()
    add_executable(BitScrape main.cpp)
endif()

# Link with component libraries
target_link_libraries(BitScrape
        PRIVATE
        dht_hunter_types
        dht_hunter_utility
        dht_hunter_network
        dht_hunter_dht
        dht_hunter_bittorrent
        dht_hunter_unified_event
        dht_hunter_web
)

# Link with platform-specific libraries
if(APPLE)
    # Link with CoreFoundation and IOKit frameworks on macOS
    target_link_libraries(BitScrape
        PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

# Set include directories
target_include_directories(BitScrape
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Install the executable
if(APPLE)
    # For macOS bundles, installation is handled in the top-level CMakeLists.txt
else()
    install(TARGETS BitScrape
            RUNTIME DESTINATION bin
    )
endif()
