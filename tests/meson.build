tests = [
  'hinoko-enum',
  'fw-iso-ctx',
  'fw-iso-rx-single',
  'fw-iso-rx-multiple',
  'fw-iso-tx',
]

objdir = join_paths(meson.build_root(), 'src')

envs = environment()
envs.append('LD_LIBRARY_PATH', objdir, separator : ':')
envs.append('GI_TYPELIB_PATH', objdir, separator : ':')

foreach test : tests
  name = test
  script = test
  prog = find_program(script)
  test(name, prog,
    env: envs,
  )
endforeach
