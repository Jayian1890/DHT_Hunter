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

# Simple executable for initial testing
executable('bitscrape_cli',
  'src/main.cpp',
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

# Installation
if is_macos
  # macOS app bundle installation
  install_data(
    'resources/macos/AppIcon.icns',
    install_dir: join_paths(get_option('datadir'), 'BitScrape.app/Contents/Resources')
  )
endif
