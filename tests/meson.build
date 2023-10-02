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

ld_library_paths = [builddir]
gi_typelib_path = [builddir]
if hinawa_is_subproject
  ld_library_paths += hinawa_gir_dir
  gi_typelib_path += hinawa_gir_dir
endif

envs = environment()
envs.append('LD_LIBRARY_PATH', ld_library_paths, separator : ':')
envs.append('GI_TYPELIB_PATH', gi_typelib_path, separator : ':')

foreach test : tests
  name = test
  script = test
  prog = find_program(script)
  test(name, prog,
    env: envs,
    depends: hinoko_gir,
  )
endforeach
