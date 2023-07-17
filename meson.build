project('hinoko', 'c',
  version: '0.9.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.46.0',
)

version = meson.project_version().split('.')
major_version = version[0].to_int()
minor_version = version[1].to_int()
micro_version = version[2].to_int()

hinawa_dependency = dependency('hinawa',
  version: '>=2.6.0'
)

subdir('src')
subdir('tests')

if get_option('doc')
  subdir('doc')
endif
