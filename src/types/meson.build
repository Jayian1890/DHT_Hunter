# Types module

dht_hunter_types_sources = [
  'node_id.cpp',
  'info_hash.cpp',
  'endpoint.cpp',
  'dht_types.cpp',
  'message_types.cpp',
  'event_types.cpp',
  'info_hash_metadata.cpp',
]

dht_hunter_types_lib = library('dht_hunter_types',
  dht_hunter_types_sources,
  include_directories: inc_dirs,
  dependencies: [
    fmt_dep,
  ],
  install: true,
)

dht_hunter_types_dep = declare_dependency(
  link_with: dht_hunter_types_lib,
  include_directories: inc_dirs,
  dependencies: [
    fmt_dep,
  ],
)
