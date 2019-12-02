project('hinoko', 'c',
  version: '0.3.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.42.0',
)

version = meson.project_version().split('.')
major_version = version[0].to_int()
minor_version = version[1].to_int()
micro_version = version[2].to_int()

gnome = import('gnome')

conf = configuration_data()
conf.set('VERSION', meson.project_version())

inc = include_directories('src')

subdir('src')
subdir('tests')
subdir('doc')
