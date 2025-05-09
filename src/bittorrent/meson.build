# BitTorrent library

dht_hunter_bittorrent_sources = [
  'bt_message_handler.cpp',
  'metadata/metadata_exchange.cpp',
  'metadata/metadata_acquisition_manager.cpp',
  'metadata/peer_health_tracker.cpp',
  'metadata/dht_metadata_provider.cpp',
  'metadata/connection_pool.cpp',
  'tracker/http_tracker.cpp',
  'tracker/udp_tracker.cpp',
  'tracker/tracker_manager.cpp',
]

bittorrent_deps = [
  dht_hunter_core_dep,
  dht_hunter_types_dep,
  dht_hunter_utility_dep,
  dht_hunter_network_dep,
  dht_hunter_dht_dep,
  dht_hunter_bencode_dep,
]

dht_hunter_bittorrent_lib = library('dht_hunter_bittorrent',
  dht_hunter_bittorrent_sources,
  include_directories: inc_dirs,
  dependencies: bittorrent_deps,
  install: true,
)

dht_hunter_bittorrent_dep = declare_dependency(
  link_with: dht_hunter_bittorrent_lib,
  include_directories: inc_dirs,
  dependencies: bittorrent_deps,
)
