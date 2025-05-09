# Web library

dht_hunter_web_sources = [
  'web_server.cpp',
  'web_bundle_manager.cpp',
  'api/config_api_handler.cpp',
]

web_deps = [
  dht_hunter_network_dep,
  dht_hunter_dht_dep,
  dht_hunter_utility_dep,
]

dht_hunter_web_lib = library('dht_hunter_web',
  dht_hunter_web_sources,
  include_directories: inc_dirs,
  dependencies: web_deps,
  install: true,
)

dht_hunter_web_dep = declare_dependency(
  link_with: dht_hunter_web_lib,
  include_directories: inc_dirs,
  dependencies: web_deps,
)
