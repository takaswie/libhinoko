inc_dir = '@0@-@1@.0'.format(meson.project_name(), major_version)

sources = [
]

headers = [
]

mapfile = 'hinoko.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library(meson.project_name(),
  sources: sources + headers,
  version: meson.project_version(),
  soversion: major_version,
  install: true,
  link_args : vflag,
  link_depends : mapfile,
)

install_headers(headers,
  subdir: join_paths(get_option('prefix'), get_option('includedir'), inc_dir),
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
  subdirs: meson.project_name(),
)
