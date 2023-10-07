tests = [
  'hinoko-enum',
  'fw-iso-ctx',
  'fw-iso-ir-single',
  'fw-iso-ir-multiple',
  'fw-iso-it',
  'fw-iso-resource',
  'fw-iso-resource-auto',
  'fw-iso-resource-once',
  'hinoko-functions',
]

builddirs = []
builddirs += join_paths(meson.project_build_root(), 'src')
if hinawa_is_subproject
  builddirs += hinawa_gir_dir
endif

envs = environment()
envs.append('LD_LIBRARY_PATH', builddirs, separator : ':')
envs.append('GI_TYPELIB_PATH', builddirs, separator : ':')

foreach test : tests
  name = test
  script = test
  prog = find_program(script)
  test(name, prog,
    env: envs,
    depends: hinoko_gir,
  )
endforeach
