project('text-edit', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20']
)

pwd = meson.current_source_dir()
cc = meson.get_compiler('cpp')

text_edit_files = [
    'src/main.cpp',
    'src/document.cpp'
]

text_edit_inc = include_directories(
    'src'
)

executable('text-edit',
    'src/main.cpp',
    text_edit_files,
    include_directories: [ text_edit_inc ]
)

