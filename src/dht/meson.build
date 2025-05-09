# DHT library

dht_hunter_dht_sources = [
  # Network components
  'network/socket_manager.cpp',
  'network/message_sender.cpp',
  'network/message_handler.cpp',
  'network/message.cpp',
  'network/query_message.cpp',
  'network/response_message.cpp',
  'network/error_message.cpp',

  # Routing components
  'routing/routing_manager.cpp',
  'routing/components/base_routing_component.cpp',
  'routing/components/node_verifier_component.cpp',
  'routing/components/bucket_refresh_component.cpp',

  # Bootstrap components
  'bootstrap/bootstrapper.cpp',
  'bootstrap/components/base_bootstrap_component.cpp',
  'bootstrap/components/bootstrap_manager.cpp',

  # Crawler components
  'crawler/crawler.cpp',
  'crawler/components/base_crawler_component.cpp',
  'crawler/components/crawler_config.cpp',
  'crawler/components/crawler_statistics.cpp',
  'crawler/components/crawler_manager.cpp',

  # Service components
  'services/statistics_service.cpp',

  # Extension components
  'extensions/factory/extension_factory.cpp',
  'extensions/implementations/mainline_dht.cpp',
  'extensions/implementations/kademlia_dht.cpp',
  'extensions/implementations/azureus_dht.cpp',
]

dht_deps = [
  dht_hunter_types_dep,
  dht_hunter_utility_dep,
  dht_hunter_network_dep,
  dht_hunter_bencode_dep,
  bitscrape_utils_dep,
]

dht_hunter_dht_lib = library('dht_hunter_dht',
  dht_hunter_dht_sources,
  include_directories: inc_dirs,
  dependencies: dht_deps,
  install: true,
)

dht_hunter_dht_dep = declare_dependency(
  link_with: dht_hunter_dht_lib,
  include_directories: inc_dirs,
  dependencies: dht_deps,
)
