# Unified event library

dht_hunter_unified_event_sources = [
  # Core components
  'event.cpp',

  # Event bus components
  'components/base_event_bus.cpp',
  'components/event_bus.cpp',

  # Processors
  'processors/components/base_event_processor.cpp',
  'processors/logging_processor.cpp',
  'processors/component_processor.cpp',
  'processors/statistics_processor.cpp',

  # Adapters
  'adapters/components/base_event_adapter.cpp',
  'adapters/dht_event_adapter.cpp',
]

unified_event_deps = [
  dht_hunter_types_dep,
  dht_hunter_utility_dep,
]

dht_hunter_unified_event_lib = library('dht_hunter_unified_event',
  dht_hunter_unified_event_sources,
  include_directories: inc_dirs,
  dependencies: unified_event_deps,
  install: true,
)

dht_hunter_unified_event_dep = declare_dependency(
  link_with: dht_hunter_unified_event_lib,
  include_directories: inc_dirs,
  dependencies: unified_event_deps,
)
