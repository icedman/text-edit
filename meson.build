project('text-edit', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20']
)

pwd = meson.current_source_dir()
cc = meson.get_compiler('cpp')

add_global_arguments('-D_XOPEN_SOURCE_EXTENDED=1', language: 'cpp')

superstring_files = [
    'superstring/src/core/encoding-conversion.cc',
    'superstring/src/core/libmba-diff.cc',
    'superstring/src/core/marker-index.cc',
    'superstring/src/core/patch.cc',
    'superstring/src/core/point.cc',
    'superstring/src/core/range.cc',
    'superstring/src/core/regex.cc',
    'superstring/src/core/text-buffer.cc',
    'superstring/src/core/text.cc',
    'superstring/src/core/text-diff.cc',
    'superstring/src/core/text-slice.cc'
]

superstring_includes = include_directories(
    'superstring/src',
    'superstring/src/core',
    'superstring/vendor/libcxx',
)

curses_dep = dependency('ncursesw', required: false, disabler: true)
if not curses_dep.found()
  curses_root = get_option('curses_root')
  curses_lib = cc.find_library('cursesw', dirs : curses_root, required : false, disabler: true)
  curses_dep = declare_dependency(include_directories: curses_root, dependencies: curses_lib)
endif

if get_option('build_curses')
executable('text-edit',
    'src/main.cpp',
    'src/input.cpp',
    'src/utf8.cpp',
    superstring_files,
    include_directories: [ 'src', superstring_includes ],
    dependencies: [ curses_dep ]
)
endif

