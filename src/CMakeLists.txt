# Add all components in the correct dependency order
add_subdirectory(types)     # Types component
add_subdirectory(utility)   # Utility functions
add_subdirectory(network)   # Network component
add_subdirectory(bencode)   # Bencode component
add_subdirectory(dht)       # DHT component
add_subdirectory(bittorrent) # BitTorrent component
add_subdirectory(unified_event) # Unified event system
add_subdirectory(web)       # Web UI component
add_subdirectory(core)      # Core application components

# Create two targets: one regular executable and one .app bundle

# Regular executable (non-.app)
add_executable(BitScrape_cli main.cpp)
set_target_properties(BitScrape_cli PROPERTIES
    OUTPUT_NAME "bitscrape"
    MACOSX_BUNDLE FALSE
)

# App bundle (only on macOS)
if(APPLE)
    # Set the icon file path
    set(APP_ICON_MACOSX "${CMAKE_CURRENT_SOURCE_DIR}/../resources/macos/AppIcon.icns")

    # Create the .app bundle executable with the icon file
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
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025 BitScrape. All rights reserved."
        MACOSX_BUNDLE_INFO_STRING "BitScrape - BitTorrent DHT crawler and analyzer"
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
    # On non-macOS platforms, just create the regular executable
    add_executable(BitScrape main.cpp)
endif()

# Link with component libraries for both targets
foreach(target BitScrape BitScrape_cli)
    target_link_libraries(${target}
        PRIVATE
        dht_hunter_core_impl
        # Remove these as they're already linked through dht_hunter_core_impl
        # dht_hunter_types
        # dht_hunter_utility
        # dht_hunter_network
        # dht_hunter_dht
        # dht_hunter_bittorrent
        # dht_hunter_unified_event
        # dht_hunter_web
    )

    # Set include directories
    target_include_directories(${target}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
endforeach()

# Link with platform-specific libraries
if(APPLE)
    # Link with CoreFoundation and IOKit frameworks on macOS
    target_link_libraries(BitScrape
        PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )

    target_link_libraries(BitScrape_cli
        PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

# Install the executables
if(APPLE)
    # For macOS bundles, installation is handled in the top-level CMakeLists.txt
    install(TARGETS BitScrape_cli
            RUNTIME DESTINATION bin
    )
else()
    install(TARGETS BitScrape BitScrape_cli
            RUNTIME DESTINATION bin
    )
endif()
