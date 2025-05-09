# Find required dependencies
thread_dep = dependency('threads')

# Create include directories
inc_dirs = include_directories('include')

# Event system library
event_system_sources = [
  'src/event_system/event.cpp',
  'src/event_system/event_bus.cpp',
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
  'src/network/udp_socket.cpp',
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
