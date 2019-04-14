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
