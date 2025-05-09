# Utils module tests

# Common Utils tests
test_common_utils = executable('test_common_utils',
  'test_common_utils.cpp',
  include_directories: inc_dirs,
  dependencies: [
    bitscrape_utils_dep,
  ],
)

test('Common Utils Test', test_common_utils)

# Domain Utils tests
test_domain_utils = executable('test_domain_utils',
  'test_domain_utils.cpp',
  include_directories: inc_dirs,
  dependencies: [
    bitscrape_utils_dep,
    dht_hunter_types_dep,
    dht_hunter_unified_event_dep,
  ],
)

test('Domain Utils Test', test_domain_utils)

# System Utils tests
test_system_utils = executable('test_system_utils',
  'test_system_utils.cpp',
  include_directories: inc_dirs,
  dependencies: [
    bitscrape_utils_dep,
  ],
)

test('System Utils Test', test_system_utils)
