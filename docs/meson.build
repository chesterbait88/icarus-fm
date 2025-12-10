
install_man(
  'nemo.1',
  'nemo-desktop.1',
  'nemo-connect-server.1',
)

if gtkdoc_enabled
  subdir('reference')
endif

# Install user documentation
install_data(
  'video-preview-setup.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'nemo')
)

# Just for Dist
extraMans = [
  'dnd.txt',
  'key_mouse_navigation.txt',
  'load-states.dia',
  'nemo-io.txt',
  'style-guide.html',
]
