configure_file(
  input : 'version.xml.in',
  output : 'version.xml',
  configuration : conf,
)

images = [
  'overview.png',
]

private_headers = [
  'internal.h',
]

gnome.gtkdoc('hinoko',
  src_dir : inc,
  main_xml : 'hinoko-docs.xml',
  html_assets: images,
  scan_args: [
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  gobject_typesfile: 'hinoko.types',
  dependencies: myself_dep,
  install : true,
)
