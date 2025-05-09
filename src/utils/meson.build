# Consolidated utilities module

bitscrape_utils_sources = [
  'common_utils.cpp',
  'domain_utils.cpp',
  'system_utils.cpp',
  'dht_core_utils.cpp',
  'misc_utils.cpp',
  'config_utils.cpp',
]

bitscrape_utils_lib = library('bitscrape_utils',
  bitscrape_utils_sources,
  include_directories: inc_dirs,
  dependencies: [
    thread_dep,
    fmt_dep,
  ],
  install: true,
)

bitscrape_utils_dep = declare_dependency(
  link_with: bitscrape_utils_lib,
  include_directories: inc_dirs,
)
