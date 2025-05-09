project('BitScrape', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++23',
    'warning_level=2',
    'buildtype=debug',
  ]
)

# Add compiler flags
cpp = meson.get_compiler('cpp')
add_project_arguments('-DFMT_SHARED', language: 'cpp')

# Platform detection
is_windows = host_machine.system() == 'windows'
is_macos = host_machine.system() == 'darwin'
is_linux = host_machine.system() == 'linux'

# Enable AddressSanitizer if requested
if get_option('enable_asan')
  if is_macos or is_linux
    add_project_arguments('-fsanitize=address', '-fno-omit-frame-pointer', language: 'cpp')
    add_project_link_arguments('-fsanitize=address', language: 'cpp')
  endif
endif

# Find required dependencies
thread_dep = dependency('threads')
fmt_dep = dependency('fmt')
curl_dep = dependency('libcurl')

# Platform-specific dependencies
platform_deps = []
if is_macos
  # macOS frameworks
  platform_deps += [
    dependency('appleframeworks', modules: ['CoreFoundation', 'IOKit'])
  ]
endif

if is_windows
  # Windows libraries
  platform_deps += [
    cpp.find_library('ws2_32')
  ]
endif

# Create a common include directory variable
inc_dirs = include_directories('include')

# Find all source files recursively
all_sources = []
subdir_list = [
  'src/types',
  'src/utility',
  'src/utils',
  'src/network',
  'src/bencode',
  'src/dht',
  'src/bittorrent',
  'src/unified_event',
  'src/web',
  'src/core'
]

foreach subdir : subdir_list
  subdir_sources = run_command('find', subdir, '-name', '*.cpp', check: false).stdout().strip().split('\n')
  if subdir_sources[0] != ''
    all_sources += subdir_sources
  endif
endforeach

# Create a single executable with all sources
executable('bitscrape_cli',
  ['src/main.cpp'] + all_sources,
  include_directories: inc_dirs,
  dependencies: [
    thread_dep,
    fmt_dep,
    curl_dep,
    platform_deps,
  ],
  cpp_args: ['-DCLI_BUILD'],
  install: true,
  install_dir: get_option('bindir'),
)

# Create macOS app bundle if requested
if is_macos and get_option('build_app_bundle')
  # Set the icon file path
  app_icon_macosx = meson.project_source_root() / 'resources/macos/AppIcon.icns'

  # Create the .app bundle executable
  app_executable = executable('BitScrape',
    ['src/main.cpp'] + all_sources,
    include_directories: inc_dirs,
    dependencies: [
      thread_dep,
      fmt_dep,
      curl_dep,
      platform_deps,
    ],
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
endif


