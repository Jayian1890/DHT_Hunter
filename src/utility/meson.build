# Utility library (legacy components)

dht_hunter_utility_sources = [
  # Common utilities
  'common/common_utils.cpp',
  
  # Random utilities
  'random/random_utils.cpp',
  
  # JSON utilities
  'json/json.cpp',
  
  # Configuration utilities
  'config/configuration_manager.cpp',
]

dht_hunter_utility_lib = library('dht_hunter_utility',
  dht_hunter_utility_sources,
  include_directories: inc_dirs,
  dependencies: [
    dht_hunter_types_dep,
    bitscrape_utils_dep,
  ],
  install: true,
)

dht_hunter_utility_dep = declare_dependency(
  link_with: dht_hunter_utility_lib,
  include_directories: inc_dirs,
  dependencies: [
    dht_hunter_types_dep,
    bitscrape_utils_dep,
  ],
)
