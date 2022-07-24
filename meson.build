project('guppirawc99', 'c',
  version: '1.6.3',
  default_options: [
    'c_std=c99',
    'buildtype=release',
    'b_ndebug=if-release'
  ]
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

src_lst = []
inc_lst = [
	include_directories('include'),
]
dep_lst = [
  m_dep,
  subproject('fitsheaderc99').get_variable('lib_fitsheader_dep'),
]

subdir('include')
subdir('src')

lib_guppiraw = library(
  'guppiraw',
  src_lst,
  include_directories: inc_lst,
  dependencies: dep_lst,
  install: true,
)

lib_guppiraw_dep = declare_dependency(
  include_directories: inc_lst,
  dependencies: dep_lst,
  link_with: lib_guppiraw,
)

subdir('tests')