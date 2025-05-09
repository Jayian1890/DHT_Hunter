project('bencode', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debug',
  ]
)

# Find required dependencies
thread_dep = dependency('threads')

# Create include directories
inc_dirs = include_directories('include')

# Bencode library
bencode_sources = [
  'src/bencode/bencode.cpp',
]

bencode_lib = library('bencode',
  bencode_sources,
  include_directories : inc_dirs,
  dependencies : [thread_dep],
  install : true,
)

bencode_dep = declare_dependency(
  link_with : bencode_lib,
  include_directories : inc_dirs,
)

# Example executable
if get_option('build_examples')
  bencode_example = executable('bencode_example',
    'examples/bencode_example.cpp',
    include_directories : inc_dirs,
    dependencies : [bencode_dep],
    install : true,
  )
endif
