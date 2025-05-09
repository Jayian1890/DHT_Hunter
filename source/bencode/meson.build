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
