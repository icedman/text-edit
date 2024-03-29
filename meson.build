project('text-edit', 'c', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20']
)

pwd = meson.current_source_dir()
cc = meson.get_compiler('cpp')

add_global_arguments('-DENABLE_LOG=1', language: 'cpp')
add_global_arguments('-DENABLE_JS=1', language: 'cpp')
# add_global_arguments('-DDISABLE_ADD_GRAMMAR_THREADS=1', language: 'cpp')

#add_global_arguments('-DDISABLE_RESOURCE_CACHING=1', language: 'cpp')
add_global_arguments('-D_XOPEN_SOURCE_EXTENDED=1', language: 'cpp')
add_global_arguments('-Wno-sign-compare', language: 'cpp')
add_global_arguments('-Wno-unused-variable', language: 'cpp')
add_global_arguments('-Wno-unused-parameter', language: 'cpp')
add_global_arguments('-Wno-unused-parameter', language: 'c')

quickjs_dep = dependency('quickjs', required: false, disabler: true)
if not quickjs_dep.found()
    quickjs_lib = cc.find_library('quickjs', required: true, dirs:['/usr/lib/quickjs', '/usr/local/lib/quickjs'])
    quickjs_dep = declare_dependency(dependencies: [ quickjs_lib ], include_directories: ['/usr/include/quickjs'])
endif

curses_dep = dependency('ncursesw', required: false, disabler: true)
if not curses_dep.found()
  curses_root = get_option('curses_root')
  curses_lib = cc.find_library('cursesw', dirs : curses_root, required : true, disabler: true)
  curses_dep = declare_dependency(include_directories: curses_root, dependencies: curses_lib)
endif

superstring_files = [
    'libs/superstring/src/core/encoding-conversion.cc',
    'libs/superstring/src/core/libmba-diff.cc',
    'libs/superstring/src/core/marker-index.cc',
    'libs/superstring/src/core/patch.cc',
    'libs/superstring/src/core/point.cc',
    'libs/superstring/src/core/range.cc',
    'libs/superstring/src/core/regex.cc',
    'libs/superstring/src/core/text-buffer.cc',
    'libs/superstring/src/core/text.cc',
    'libs/superstring/src/core/text-diff.cc',
    'libs/superstring/src/core/text-slice.cc'
]

superstring_includes = include_directories(
    'libs/superstring/src',
    'libs/superstring/src/core',
    'libs/superstring/vendor/libcxx',
)

onigmo_files = [
    'libs/Onigmo/regcomp.c',
    'libs/Onigmo/regexec.c',
    'libs/Onigmo/regparse.c',
    'libs/Onigmo/regsyntax.c',
    'libs/Onigmo/st.c',
    'libs/Onigmo/regenc.c',
    'libs/Onigmo/regext.c',
    'libs/Onigmo/regposerr.c',
    'libs/Onigmo/regtrav.c',
    'libs/Onigmo/regerror.c',
    'libs/Onigmo/reggnu.c',
    'libs/Onigmo/regposix.c',
    'libs/Onigmo/regversion.c',
    'libs/Onigmo/enc/ascii.c',
    #'libs/Onigmo/enc/euc_kr.c',
    #'libs/Onigmo/enc/iso_8859_10.c',
    #'libs/Onigmo/enc/iso_8859_16.c',
    #'libs/Onigmo/enc/iso_8859_5.c',
    #'libs/Onigmo/enc/koi8_r.c',
    'libs/Onigmo/enc/us_ascii.c',
    'libs/Onigmo/enc/utf_8.c',
    'libs/Onigmo/enc/windows_1254.c',
    'libs/Onigmo/enc/big5.c',
    'libs/Onigmo/enc/euc_tw.c',
    'libs/Onigmo/enc/iso_8859_11.c',
    'libs/Onigmo/enc/iso_8859_1.c',
    'libs/Onigmo/enc/iso_8859_6.c',
    'libs/Onigmo/enc/koi8_u.c',
    'libs/Onigmo/enc/utf_16be.c',
    #'libs/Onigmo/enc/windows_1250.c',
    #'libs/Onigmo/enc/windows_1257.c',
    #'libs/Onigmo/enc/cp949.c',
    #'libs/Onigmo/enc/gb18030.c',
    'libs/Onigmo/enc/iso_8859_13.c',
    'libs/Onigmo/enc/iso_8859_2.c',
    'libs/Onigmo/enc/iso_8859_7.c',
    #'libs/Onigmo/enc/mktable.c',
    'libs/Onigmo/enc/utf_16le.c',
    #'libs/Onigmo/enc/windows_1251.c',
    #'libs/Onigmo/enc/windows_31j.c',
    #'libs/Onigmo/enc/emacs_mule.c',
    #'libs/Onigmo/enc/gb2312.c',
    'libs/Onigmo/enc/iso_8859_14.c',
    'libs/Onigmo/enc/iso_8859_3.c',
    'libs/Onigmo/enc/iso_8859_8.c',
    'libs/Onigmo/enc/shift_jis.c',
    'libs/Onigmo/enc/utf_32be.c',
    #'libs/Onigmo/enc/windows_1252.c',
    'libs/Onigmo/enc/euc_jp.c',
    #'libs/Onigmo/enc/gbk.c',
    'libs/Onigmo/enc/iso_8859_15.c',
    'libs/Onigmo/enc/iso_8859_4.c',
    'libs/Onigmo/enc/iso_8859_9.c',
    'libs/Onigmo/enc/unicode.c',
    'libs/Onigmo/enc/utf_32le.c'
    #'libs/Onigmo/enc/windows_1253.c'
]

onigmo_includes = [
    'libs/Onigmo',
    'libs/Onigmo/enc',
    'libs/Onigmo/enc/unicode'
]

jsoncpp_files = [
    'libs/jsoncpp/dist/jsoncpp.cpp'
]

jsoncpp_includes = [
    'libs/jsoncpp/dist'
]

tinyxml2_files = [
    'libs/tinyxml2/tinyxml2.cpp'
]

tinyxml2_includes = [
    'libs/tinyxml2/'
]

tm_parser_files = [
    'libs/tm-parser/textmate/textmate.cpp',
    'libs/tm-parser/textmate/parser/grammar.cpp',
    'libs/tm-parser/textmate/parser/parser.cpp',
    'libs/tm-parser/textmate/parser/pattern.cpp',
    'libs/tm-parser/textmate/parser/reader.cpp',
    'libs/tm-parser/textmate/scopes/match.cpp',
    'libs/tm-parser/textmate/scopes/parse.cpp',
    'libs/tm-parser/textmate/scopes/scope.cpp',
    'libs/tm-parser/textmate/scopes/types.cpp',
    'libs/tm-parser/textmate/theme/theme.cpp',
    'libs/tm-parser/textmate/extensions/util.cpp',
    'libs/tm-parser/textmate/extensions/extension.cpp',
    'libs/tm-parser/textmate/resources/grammars.cpp',
    'libs/tm-parser/textmate/resources/themes.cpp'
]

tm_parser_includes = [
    'libs/tm-parser/textmate',
    'libs/tm-parser/textmate/parser',
    'libs/tm-parser/textmate/scopes',
    'libs/tm-parser/textmate/theme',
    'libs/tm-parser/textmate/extensions',
    'libs/tm-parser/textmate/resources'
]

tree_sitter_files = [
    'libs/tree-sitter/lib/src/alloc.c',
    'libs/tree-sitter/lib/src/get_changed_ranges.c',
    'libs/tree-sitter/lib/src/language.c',
    'libs/tree-sitter/lib/src/lexer.c',
    'libs/tree-sitter/lib/src/node.c',
    'libs/tree-sitter/lib/src/parser.c',
    'libs/tree-sitter/lib/src/query.c',
    'libs/tree-sitter/lib/src/stack.c',
    'libs/tree-sitter/lib/src/subtree.c',
    'libs/tree-sitter/lib/src/tree.c',
    'libs/tree-sitter/lib/src/tree_cursor.c'
]

tree_sitter_includes = [
    'libs/tree-sitter/lib/include'
]

tree_sitter_grammar_files = []

tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-c/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-cpp/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-cpp/src/scanner.cc'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-c-sharp/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-c-sharp/src/scanner.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-css/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-css/src/scanner.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-html/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-html/src/scanner.cc'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-java/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-javascript/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-javascript/src/scanner.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-json/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-python/src/parser.c'
tree_sitter_grammar_files += 'libs/tree-sitter-grammars/tree-sitter-python/src/scanner.cc'

#add_global_arguments('-DPCRE2_CODE_UNIT_WIDTH=16', language: 'cpp')
#pcre2_dep = dependency('libpcre2-16', required: false, disabler: true)

executable('text-edit',
    'src/main.cpp',
    'src/cursor.cpp',
    'src/document.cpp',
    'src/autocomplete.cpp',
    'src/highlight.cpp',
    'src/js.cpp',
    'src/search.cpp',
    'src/input.cpp',
    'src/keybindings.cpp',
    'src/utf8.cpp',
    'src/treesitter.cpp',
    'src/files.cpp',
    'src/view.cpp',
    'src/menu.cpp',
    'src/editor.cpp',
    'src/ui.cpp',
    'src/render.cpp',
    superstring_files,
    # quickjs_files,
    onigmo_files,
    jsoncpp_files,
    tinyxml2_files,
    tm_parser_files,
    tree_sitter_files,
    tree_sitter_grammar_files,
    include_directories: [
        'src',
        tm_parser_includes,
        jsoncpp_includes,
        # quickjs_includes,
        tinyxml2_includes,
        onigmo_includes,
        superstring_includes,
        tree_sitter_includes
    ],
    dependencies: [ curses_dep, quickjs_dep ]
)

if get_option('build_tests')
executable('regex_test',
    'tests/regex_test.cpp',
    'src/utf8.cpp',
    superstring_files,
    onigmo_files,
    include_directories: [
        'src',
        onigmo_includes,
        superstring_includes
    ],
    dependencies: [ curses_dep ]
)
executable('keys_test',
    'tests/keys_test.cpp',
    'src/utf8.cpp',
    'src/input.cpp',
    'src/keybindings.cpp',
    superstring_files,
    onigmo_files,
    include_directories: [
        'src',
        onigmo_includes,
        superstring_includes
    ],
    dependencies: [ curses_dep ]
)
executable('explorer_test',
    'tests/explorer_test.cpp',
    'src/files.cpp',
    'libs/tm-parser/textmate/extensions/util.cpp',
    include_directories: [
        'src',
        tm_parser_includes
    ]
)
endif

