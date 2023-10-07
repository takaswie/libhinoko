# Depends on glib-2.0 and gobject-2.0.
gobject_dependency = dependency('gobject-2.0',
  version: '>=2.44.0'
)

dependencies = [
  gobject_dependency,
  hinawa_dependency,
]

sources = [
  'fw_iso_ctx.c',
  'fw_iso_ir_single.c',
  'fw_iso_ir_multiple.c',
  'fw_iso_it.c',
  'fw_iso_resource.c',
  'fw_iso_resource_auto.c',
  'fw_iso_resource_once.c',
]

headers = [
  'hinoko.h',
  'fw_iso_ctx.h',
  'fw_iso_ir_single.h',
  'fw_iso_ir_multiple.h',
  'fw_iso_it.h',
  'fw_iso_resource.h',
  'fw_iso_resource_auto.h',
  'fw_iso_resource_once.h',
  'hinoko_enum_types.h'
]

privates = [
  'fw_iso_ctx_private.h',
  'fw_iso_ctx_private.c',
  'fw_iso_resource_private.h',
  'fw_iso_resource_private.c',
]

inc_dir = meson.project_name()

gnome = import('gnome')

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinoko_sigs_marshal',
  prefix: 'hinoko_sigs_marshal',
  sources: 'hinoko_sigs_marshal.list',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  stdinc: true,
)

enums = gnome.mkenums_simple('hinoko_enums',
  sources: ['hinoko_enum_types.h'],
  identifier_prefix: 'Hinoko',
  symbol_prefix: 'hinoko',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  header_prefix: '#include <hinoko.h>',
)

mapfile = 'hinoko.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library(meson.project_name(),
  sources: sources + headers + privates + marshallers + enums,
  version: meson.project_version(),
  soversion: major_version,
  install: true,
  link_args : vflag,
  link_depends : mapfile,
  dependencies: dependencies,
)

install_headers(headers,
  subdir: inc_dir,
)

pkg = import('pkgconfig')
pkg.generate(myself,
  description: 'The library to operate OHCI 1394 hardware for isoochronous communication in IEEE 1394 bus',
  subdirs: inc_dir,
)

hinoko_gir = gnome.generate_gir(myself,
  sources: enums + headers + sources,
  nsversion: '@0@.0'.format(major_version),
  namespace: 'Hinoko',
  symbol_prefix: 'hinoko_',
  identifier_prefix: 'Hinoko',
  export_packages: 'hinoko',
  dependencies: dependencies,
  includes: [
    'GLib-2.0',
    'GObject-2.0',
    'Hinawa-3.0',
  ],
  header: 'hinoko.h',
  install: true,
)

# For wrap dependency system.
hinoko_dep = declare_dependency(
  link_with: myself,
  dependencies: dependencies,
  sources: headers + marshallers + enums + hinoko_gir,
  include_directories: include_directories('.')
)
meson.override_dependency('hinoko', hinoko_dep)
