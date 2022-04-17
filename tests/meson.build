tests = [
  'hinoko-enum',
  'fw-iso-ctx',
  'fw-iso-rx-single',
  'fw-iso-rx-multiple',
  'fw-iso-tx',
  'fw-iso-resource',
  'fw-iso-resource-auto',
  'fw-iso-resource-once',
]

envs = environment()
envs.append('LD_LIBRARY_PATH', builddir, separator : ':')
envs.append('GI_TYPELIB_PATH', builddir, separator : ':')

foreach test : tests
  name = test
  script = test
  prog = find_program(script)
  test(name, prog,
    env: envs,
  )
endforeach
