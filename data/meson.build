
################################################################################
dataConf = configuration_data()
dataConf.set('VERSION', meson.project_version())

desktopFiles = [
  configure_file(
    input : 'nemo-autorun-software.desktop.in',
    output: 'icarus-fm-autorun-software.desktop',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo-autostart.desktop.in',
    output: 'icarus-fm-autostart.desktop',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo.desktop.in',
    output: 'icarus-fm.desktop',
    configuration: dataConf,
  )
]

install_data(desktopFiles,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

dbusFiles = [
  configure_file(
    input : 'nemo.service.in',
    output: 'org.IcarusFM.service',
    configuration: dataConf,
  ),

  configure_file(
    input : 'nemo.FileManager1.service.in',
    output: 'icarus-fm.FileManager1.service',
    configuration: dataConf,
  ),
]

install_data(dbusFiles,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services')
)

################################################################################

mimeFile_translated = custom_target('icarus-fm.xml',
  input : ['nemo.xml.in'],
  output: 'icarus-fm.xml',
  command: ['intltool-merge',
    '--quiet',
    '--xml-style',
    '--utf8',
    po_subdir, '@INPUT@', '@OUTPUT@'
  ],
  install: true,
  install_dir: join_paths(get_option('datadir'), 'mime', 'packages'),
  build_by_default: true,
)

################################################################################
dataConf = configuration_data()
dataConf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

rootPolicyFile = configure_file(
  input : 'org.nemo.root.policy.in',
  output: 'org.icarus-fm.root.policy',
  configuration: dataConf,
)

install_data(rootPolicyFile,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions')
)

foreach lang_file: ['nemo_action.lang', 'nemo_search_helper.lang']
  foreach sv_dir: ['gtksourceview-2.0', 'gtksourceview-3.0', 'gtksourceview-4']
    install_data(lang_file,
      install_dir: join_paths(get_option('datadir'), sv_dir, 'language-specs'),
      rename: lang_file.replace('nemo', 'icarus-fm')
    )
  endforeach
endforeach

################################################################################

subdir('icons')
