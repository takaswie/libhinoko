ext_contents = [
  'overview.md',
  'overview.png',
]

conf = configuration_data()
conf.set('VERSION', meson.project_version())

doc_toml = configure_file(
  input: 'hinoko.toml.in',
  output: 'hinoko.toml',
  configuration: conf,
)

dependency('gi-docgen',
  version: '>= 2021.8',
  fallback: ['gi-docgen', 'dummy_dep'],
)
gidocgen = find_program('gi-docgen')

subproject_dependent_args = []
if hinawa_is_subproject
  subproject_dependent_args = ['--add-include-path', hinawa_gir_dir]
endif

doc_dir = meson.project_name()

custom_target('hinoko-doc',
  input: [ doc_toml, hinoko_gir[0] ],
  output: doc_dir,
  command: [
    gidocgen,
    'generate',
    subproject_dependent_args,
    '--no-namespace-dir',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depend_files: ext_contents,
  build_by_default: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'doc'),
)
