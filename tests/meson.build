test_file_stempath = './guppirawc99_test_file'

# raise priority of `writer` test above the others
# to ensure it runs first, but below 0 so subproject
# tests run first. Its output will be the input of 
# the other tests
test('write', executable(
  'write', ['write.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: [test_file_stempath],
  timeout: false,
  priority : -1
)

test('header', executable(
  'header', ['header.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  priority : -1
)

test('read_blockheader', executable(
  'read_blockheader', ['read_blockheader.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: [test_file_stempath + '.0000.raw'],
  is_parallel : false,
  priority : -2
)
test('peek_file_benchmark', executable(
  'peek_file_benchmark', ['peek_file_benchmark.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: [test_file_stempath + '.0000.raw'],
  is_parallel : false,
  priority : -2
)
test('iterate', executable(
  'iterate', ['iterate.c'],
	  dependencies: lib_guppiraw_dep,
  ),
  args: [test_file_stempath],
  is_parallel : false,
  timeout: false,
  priority : -3
)

executable(
'key_comparison', ['key_comparison.c'],
  dependencies: lib_guppiraw_dep,
)
