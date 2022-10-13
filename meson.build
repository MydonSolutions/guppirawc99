project('guppirawc99', 'c',
  version: '1.9.4',
  default_options: [
    'c_std=c99',
    'buildtype=release',
    'b_ndebug=if-release'
  ]
)

add_project_arguments('-D_POSIX_C_SOURCE=199309L', language : 'c')

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
  'guppirawc99',
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

pkg = import('pkgconfig')
pkg.generate(
  lib_guppiraw,
  libraries: dep_lst,
)

subdir('tests')