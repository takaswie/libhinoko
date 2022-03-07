inc_dir = '@0@-@1@.0'.format(meson.project_name(), major_version)

# Depends on glib-2.0 and gobject-2.0.
gobject = dependency('gobject-2.0',
  version: '>=2.34.0'
)
dependencies = [
  gobject,
]

sources = [
  'fw_iso_ctx.c',
  'fw_iso_rx_single.c',
  'fw_iso_rx_multiple.c',
  'fw_iso_tx.c',
  'cycle_timer.c',
  'fw_iso_resource.c',
  'fw_iso_resource_auto.c',
]

headers = [
  'hinoko.h',
  'fw_iso_ctx.h',
  'fw_iso_rx_single.h',
  'fw_iso_rx_multiple.h',
  'fw_iso_tx.h',
  'cycle_timer.h',
  'fw_iso_resource.h',
  'fw_iso_resource_auto.h',
]

inc_dir = meson.project_name()

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinoko_sigs_marshal',
  prefix: 'hinoko_sigs_marshal',
  sources: 'hinoko_sigs_marshal.list',
  stdinc: true,
)
sources += marshallers

enums = gnome.mkenums_simple('hinoko_enums',
  sources: ['hinoko_enum_types.h'],
  identifier_prefix: 'Hinoko',
  symbol_prefix: 'hinoko',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  header_prefix: '#include <hinoko.h>',
)
headers += 'hinoko_enum_types.h'
sources += enums

mapfile = 'hinoko.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library(meson.project_name(),
  sources: sources + headers,
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
pkg.generate(
  version: meson.project_version(),
  libraries: myself,
  requires: [
    'glib-2.0',
    'gobject-2.0',
  ],
  name: meson.project_name(),
  description: 'A library to operate OHCI 1394 controller for isochronous stream',
  filebase: meson.project_name(),
  subdirs: inc_dir,
)

hinoko_gir = gnome.generate_gir(myself,
  sources: sources + headers,
  nsversion: '@0@.0'.format(major_version),
  namespace: 'Hinoko',
  symbol_prefix: 'hinoko_',
  identifier_prefix: 'Hinoko',
  export_packages: 'hinoko',
  dependencies: dependencies,
  includes: [
    'GLib-2.0',
    'GObject-2.0',
  ],
  header: 'hinoko.h',
  install: true,
)

myself_dep = declare_dependency(
  link_with: myself,
  dependencies: dependencies,
)
