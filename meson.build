project('hinoko', 'c',
  version: '1.0.2',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.58.0',
)

version = meson.project_version().split('.')
major_version = version[0].to_int()
minor_version = version[1].to_int()
micro_version = version[2].to_int()

hinawa_dependency = dependency('hinawa',
  version: '>=4.0.0'
)

hinawa_is_subproject = hinawa_dependency.type_name() != 'pkgconfig'
if hinawa_is_subproject
  hinawa_gir_dir = join_paths(meson.global_build_root(), 'subprojects', 'libhinawa', 'src')
endif

subdir('src')
subdir('tests')

if get_option('doc')
  subdir('doc')
endif
