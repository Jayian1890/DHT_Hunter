project('EventDrivenSystem', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debug',
  ]
)

# Find required dependencies
thread_dep = dependency('threads')

# Create include directories
inc_dirs = include_directories('source/include')

# Event system library
event_system_sources = [
  'source/src/event_system/event.cpp',
  'source/src/event_system/event_bus.cpp',
]

event_system_lib = library('event_system',
  event_system_sources,
  include_directories : inc_dirs,
  dependencies : [thread_dep],
  install : true,
)

event_system_dep = declare_dependency(
  link_with : event_system_lib,
  include_directories : inc_dirs,
)

# Network library
network_sources = [
  'source/src/network/udp_socket.cpp',
]

network_lib = library('network',
  network_sources,
  include_directories : inc_dirs,
  dependencies : [event_system_dep, thread_dep],
  install : true,
)

network_dep = declare_dependency(
  link_with : network_lib,
  include_directories : inc_dirs,
)

# Example executable
event_driven_example = executable('event_driven_example',
  'source/examples/event_driven_example.cpp',
  include_directories : inc_dirs,
  dependencies : [event_system_dep, network_dep, thread_dep],
  install : true,
)
