project('text-edit', 'c', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20']
)

pwd = meson.current_source_dir()
cc = meson.get_compiler('cpp')

#add_global_arguments('-DDISABLE_RESOURCE_CACHING=1', language: 'cpp')
add_global_arguments('-D_XOPEN_SOURCE_EXTENDED=1', language: 'cpp')
add_global_arguments('-Wno-sign-compare', language: 'cpp')
add_global_arguments('-Wno-unused-variable', language: 'cpp')
add_global_arguments('-Wno-unused-parameter', language: 'cpp')
add_global_arguments('-DNO_TERM=1', language: 'cpp')
add_global_arguments('-DHAVE_CONFIG_H=1', language: 'cpp')
add_global_arguments('-fpermissive', language: 'cpp')

add_global_arguments('-DVIM_MODE=1', language: 'cpp')
add_global_arguments('-DENABLE_LOG=1', language: 'cpp')

add_global_arguments('-Wno-unused-parameter', language: 'c')
add_global_arguments('-DNO_TERM=1', language: 'c')
add_global_arguments('-DHAVE_CONFIG_H=1', language: 'c')
add_global_arguments('-fpermissive', language: 'c')

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

vim_files = [
    'libs/libvim/src/arabic.c',
    'libs/libvim/src/autocmd.c',
    'libs/libvim/src/buffer.c',
    'libs/libvim/src/change.c',
    'libs/libvim/src/blob.c',
    'libs/libvim/src/debugger.c',
    'libs/libvim/src/dict.c',
    'libs/libvim/src/diff.c',
    'libs/libvim/src/digraph.c',
    'libs/libvim/src/edit.c',
    'libs/libvim/src/eval.c',
    'libs/libvim/src/evalfunc.c',
    'libs/libvim/src/ex_cmds.c',
    'libs/libvim/src/ex_cmds2.c',
    'libs/libvim/src/ex_docmd.c',
    'libs/libvim/src/ex_eval.c',
    'libs/libvim/src/ex_getln.c',
    'libs/libvim/src/fileio.c',
    'libs/libvim/src/findfile.c',
    'libs/libvim/src/fold.c',
    'libs/libvim/src/getchar.c',
    'libs/libvim/src/hashtab.c',
    'libs/libvim/src/indent.c',
    'libs/libvim/src/libvim.c',
    'libs/libvim/src/list.c',
    'libs/libvim/src/mark.c',
    'libs/libvim/src/memline.c',
    'libs/libvim/src/message2.c',
    'libs/libvim/src/misc1.c',
    'libs/libvim/src/misc2.c',
    'libs/libvim/src/move.c',
    'libs/libvim/src/mbyte.c',
    'libs/libvim/src/normal.c',
    'libs/libvim/src/ops.c',
    'libs/libvim/src/option.c',
    'libs/libvim/src/os_unix.c',
    'libs/libvim/src/auto/pathdef.c',
    'libs/libvim/src/pty.c',
    'libs/libvim/src/quickfix.c',
    'libs/libvim/src/regexp.c',
    'libs/libvim/src/screen.c',
    'libs/libvim/src/sds.c',
    'libs/libvim/src/search.c',
    'libs/libvim/src/sha256.c',
    'libs/libvim/src/sign.c',
    'libs/libvim/src/state_insert_literal.c',
    'libs/libvim/src/state_machine.c',
    'libs/libvim/src/syntax.c',
    'libs/libvim/src/tag.c',
    'libs/libvim/src/term.c',
    'libs/libvim/src/ui.c',
    'libs/libvim/src/undo.c',
    'libs/libvim/src/usercmd.c',
    'libs/libvim/src/userfunc.c',
    'libs/libvim/src/version.c',
    'libs/libvim/src/window.c',
    # $(GUI_OBJ) \,
    # $(LUA_OBJ) \,
    # $(MZSCHEME_OBJ) \,
    # $(PERL_OBJ) \,
    # $(PYTHON_OBJ) \,
    # $(PYTHON3_OBJ) \,
    # $(RUBY_OBJ) \,
    # $(OS_EXTRA_OBJ) \,
    # $(CHANNEL_OBJ) \,
    'libs/libvim/src/channel.c',
    # $(XDIFF_OBJS),
# XDIFF_OBJS
    'libs/libvim/src/xdiff/xdiffi.c',
    'libs/libvim/src/xdiff/xemit.c',
    'libs/libvim/src/xdiff/xprepare.c',
    'libs/libvim/src/xdiff/xutils.c',
    'libs/libvim/src/xdiff/xhistogram.c',
    'libs/libvim/src/xdiff/xpatience.c',
#  OBJ_MAIN
    'libs/libvim/src/charset.c',
    'libs/libvim/src/json.c',
    'libs/libvim/src/main.c',
    'libs/libvim/src/memfile.c',
    'libs/libvim/src/message.c'
]

vim_includes = [
    'libs/libvim/src',
    'libs/libvim/src/proto',
    'libs/libvim/src/xdiff'
]

curses_dep = dependency('ncursesw', required: false, disabler: true)
if not curses_dep.found()
  curses_root = get_option('curses_root')
  curses_lib = cc.find_library('cursesw', dirs : curses_root, required : false, disabler: true)
  curses_dep = declare_dependency(include_directories: curses_root, dependencies: curses_lib)
endif

math_lib = cc.find_library('m', required : false, disabler: true)
math_dep = declare_dependency(dependencies: math_lib)

acl_lib = cc.find_library('acl', required : false, disabler: true)
acl_dep = declare_dependency(dependencies: acl_lib)

if get_option('build_curses')
executable('text-edit',
    'src/main.cpp',
    'src/cursor.cpp',
    'src/document.cpp',
    'src/autocomplete.cpp',
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
    onigmo_files,
    jsoncpp_files,
    tinyxml2_files,
    tm_parser_files,
    tree_sitter_files,
    tree_sitter_grammar_files,
    vim_files,
    include_directories: [
        'src',
        tm_parser_includes,
        jsoncpp_includes,
        tinyxml2_includes,
        onigmo_includes,
        superstring_includes,
        tree_sitter_includes,
        vim_includes
    ],
    dependencies: [ curses_dep, math_dep, acl_dep ]
)
endif

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
    ]
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

