
# Install man pages with renamed filenames for icarus-fm
install_data('nemo.1',
  install_dir: join_paths(get_option('mandir'), 'man1'),
  rename: 'icarus-fm.1'
)
install_data('nemo-desktop.1',
  install_dir: join_paths(get_option('mandir'), 'man1'),
  rename: 'icarus-fm-desktop.1'
)
install_data('nemo-connect-server.1',
  install_dir: join_paths(get_option('mandir'), 'man1'),
  rename: 'icarus-fm-connect-server.1'
)

if gtkdoc_enabled
  subdir('reference')
endif

# Install user documentation
install_data(
  'video-preview-setup.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'icarus-fm')
)

# Just for Dist
extraMans = [
  'dnd.txt',
  'key_mouse_navigation.txt',
  'load-states.dia',
  'nemo-io.txt',
  'style-guide.html',
]
