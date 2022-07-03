test('main', executable(
  'main', ['main.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  #args: ['/mnt/buf0/synth_large.0000.raw']
)
test('peek_file_benchmark', executable(
  'peek_file_benchmark', ['peek_file_benchmark.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  #args: ['/mnt/buf0/synth_large.0000.raw']
)
test('iterate', executable(
  'iterate', ['iterate.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: ['/mnt/buf0/synth_large.0000.raw'],
  timeout: false
)
