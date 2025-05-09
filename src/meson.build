# Add all components in the correct dependency order
subdir('utils')     # Consolidated utility functions
subdir('types')     # Types component
subdir('utility')   # Utility functions (legacy)
subdir('network')   # Network component
subdir('bencode')   # Bencode component
subdir('dht')       # DHT component
subdir('bittorrent') # BitTorrent component
subdir('unified_event') # Unified event system
subdir('web')       # Web UI component
subdir('core')      # Core application components

if get_option('build_tests')
  subdir('tests')   # Tests
endif

# Create two targets: one regular executable and one .app bundle

# Regular executable (non-.app)
executable('bitscrape_cli',
  'main.cpp',
  install: true,
  install_dir: get_option('bindir'),
  dependencies: [
    dht_hunter_core_impl_dep,
    dht_hunter_types_dep,
    dht_hunter_utility_dep,
    dht_hunter_network_dep,
    dht_hunter_bencode_dep,
    dht_hunter_dht_dep,
    dht_hunter_bittorrent_dep,
    dht_hunter_unified_event_dep,
    dht_hunter_web_dep,
    bitscrape_utils_dep,
    thread_dep,
    fmt_dep,
    curl_dep,
    platform_deps,
  ],
  include_directories: inc_dirs,
  cpp_args: ['-DCLI_BUILD'],
)

# App bundle (only on macOS)
if is_macos and get_option('build_app_bundle')
  # Set the icon file path
  app_icon_macosx = meson.project_source_root() / 'resources/macos/AppIcon.icns'

  # Create the .app bundle executable
  app_executable = executable('BitScrape',
    'main.cpp',
    dependencies: [
      dht_hunter_core_impl_dep,
      dht_hunter_types_dep,
      dht_hunter_utility_dep,
      dht_hunter_network_dep,
      dht_hunter_bencode_dep,
      dht_hunter_dht_dep,
      dht_hunter_bittorrent_dep,
      dht_hunter_unified_event_dep,
      dht_hunter_web_dep,
      bitscrape_utils_dep,
      thread_dep,
      fmt_dep,
      curl_dep,
      platform_deps,
    ],
    include_directories: inc_dirs,
    install: true,
    install_dir: get_option('bindir'),
  )

  # Custom target to create app bundle structure
  custom_target('app_bundle',
    input: app_executable,
    output: 'BitScrape.app',
    command: [
      'mkdir', '-p', '@OUTPUT@/Contents/MacOS', '@OUTPUT@/Contents/Resources',
      '&&', 'cp', '@INPUT@', '@OUTPUT@/Contents/MacOS/',
      '&&', 'cp', app_icon_macosx, '@OUTPUT@/Contents/Resources/AppIcon.icns',
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('bindir'),
  )
else
  # On non-macOS platforms, just create the regular executable
  executable('bitscrape',
    'main.cpp',
    dependencies: [
      dht_hunter_core_impl_dep,
      dht_hunter_types_dep,
      dht_hunter_utility_dep,
      dht_hunter_network_dep,
      dht_hunter_bencode_dep,
      dht_hunter_dht_dep,
      dht_hunter_bittorrent_dep,
      dht_hunter_unified_event_dep,
      dht_hunter_web_dep,
      bitscrape_utils_dep,
      thread_dep,
      fmt_dep,
      curl_dep,
      platform_deps,
    ],
    include_directories: inc_dirs,
    install: true,
    install_dir: get_option('bindir'),
  )
endif
