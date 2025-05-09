# Core library

dht_hunter_core_impl_sources = [
  'application_controller.cpp',
]

core_deps = [
  dht_hunter_types_dep,
  dht_hunter_utility_dep,
  dht_hunter_network_dep,
  dht_hunter_dht_dep,
  dht_hunter_bittorrent_dep,
  dht_hunter_unified_event_dep,
  dht_hunter_web_dep,
]

dht_hunter_core_impl_lib = library('dht_hunter_core_impl',
  dht_hunter_core_impl_sources,
  include_directories: inc_dirs,
  dependencies: core_deps,
  install: true,
)

dht_hunter_core_impl_dep = declare_dependency(
  link_with: dht_hunter_core_impl_lib,
  include_directories: inc_dirs,
  dependencies: core_deps,
)
