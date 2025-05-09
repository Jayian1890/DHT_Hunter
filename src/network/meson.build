# Network library

dht_hunter_network_sources = [
  'udp_socket.cpp',
  'udp_server.cpp',
  'udp_client.cpp',
  'tcp_client.cpp',
  'ssl_client.cpp',
  'network_address.cpp',
  'http_client.cpp',
  'http_server.cpp',
  'nat/upnp_service.cpp',
  'nat/natpmp_service.cpp',
  'nat/nat_traversal_manager.cpp',
]

network_deps = [
  dht_hunter_types_dep,
  dht_hunter_utility_dep,
  platform_deps,
]

dht_hunter_network_lib = library('dht_hunter_network',
  dht_hunter_network_sources,
  include_directories: inc_dirs,
  dependencies: network_deps,
  install: true,
)

dht_hunter_network_dep = declare_dependency(
  link_with: dht_hunter_network_lib,
  include_directories: inc_dirs,
  dependencies: network_deps,
)
