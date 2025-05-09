# Bencode library

# No source files found in this directory
# Creating an empty library for now
bencode_sources = []

bencode_deps = [
  dht_hunter_types_dep,
  thread_dep,
]

# Create an interface library since there are no source files
dht_hunter_bencode_lib = declare_dependency(
  include_directories: inc_dirs,
  dependencies: bencode_deps,
)

dht_hunter_bencode_dep = dht_hunter_bencode_lib
