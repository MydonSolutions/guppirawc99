test('read_blockheader', executable(
  'read_blockheader', ['read_blockheader.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: ['/mnt/buf0/synthesized_large.0000.raw']
)
test('peek_file_benchmark', executable(
  'peek_file_benchmark', ['peek_file_benchmark.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: ['/mnt/buf0/synthesized_large.0000.raw']
)
test('iterate', executable(
  'iterate', ['iterate.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: ['/mnt/buf0/synthesized_large.0000.raw'],
  timeout: false
)
test('header', executable(
  'header', ['header.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  timeout: false
)
test('write', executable(
  'write', ['write.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  timeout: false
)

executable(
'key_comparison', ['key_comparison.c'],
  dependencies: lib_guppiraw_dep,
)
