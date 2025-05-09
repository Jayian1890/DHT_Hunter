# Bencode library

dht_hunter_bencode_sources = [
  'bencode.cpp',
]

dht_hunter_bencode_lib = library('dht_hunter_bencode',
  dht_hunter_bencode_sources,
  include_directories: inc_dirs,
  dependencies: [
    dht_hunter_core_dep,
  ],
  install: true,
)

dht_hunter_bencode_dep = declare_dependency(
  link_with: dht_hunter_bencode_lib,
  include_directories: inc_dirs,
)
