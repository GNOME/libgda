vg = find_program('valgrind', required: false)
if vg.found ()
add_test_setup('valgrind',
  exe_wrapper : [vg,
  	'--tool=memcheck',
  	'--leak-check=full',
  	'--leak-resolution=high',
  	'--show-leak-kinds=definite',
  	'--trace-children=yes',
  	'--num-callers=20',
  	'--suppressions='+join_paths(meson.current_source_dir (), 'gtk.suppression'),
  	'--log-file=vgdump'],
  timeout_multiplier : 100)
endif

test_cargs = [
	'-include',
	join_paths(gda_top_build, 'config.h'),
	'-DCHECK_FILES="'+gda_top_src+'"',
	'-DTOP_BUILD_DIR="'+gda_top_build+'"'
	]
test_cargs += general_cargs

test_headers = files([
	'raw-ddl-creator.h'
	])

inc_testsh = include_directories ('.')
inc_testsh_dep = declare_dependency (include_directories : inc_testsh)

tests_sources = files([
	'raw-ddl-creator.c',
	'test-cnc-utils.h',
	'test-cnc-utils.c',
	'data-model-errors.h',
	'data-model-errors.c'
	])

common_sources = files([
	'test-errors.h',
	'test-errors.c'
	])

tests_sources += test_headers
tests_sources += common_sources

trddlc = executable('test-ddl-creator test',
	['test-ddl-creator.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('RawDdlCreator', trddlc,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

ddlmodifiable_sqlite = executable('test-ddl-modifiable-sqlite',
	['test-ddl-modifiable-sqlite.c'],
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('DdlModifiableSqlite', ddlmodifiable_sqlite,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tbc = executable('test-bin-converter',
	['test-bin-converter.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('BinConverter', tbc,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)


tsqli = executable('test-sql-identifier',
	['test-sql-identifier.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('SqlIdentifier', tsqli,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tiq = executable('test-identifiers-quotes',
	['test-identifiers-quotes.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('IdentifiersQuotes', tiq,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tsqlb = executable('test-sql-builder',
	['test-sql-builder.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('SqlBuilder', tsqlb,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tcstrs = executable('test-connection-string-split',
	['test-connection-string-split.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ConnectionStringSplit', tcstrs,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tipar = executable('test-input-parsers',
	['test-input-parsers.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('InputParsers', tipar,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tqkl = executable('test-quark-list',
	['test-quark-list.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('QuarkList', tqkl,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tsqlr = executable('test-sql-renderer',
	['test-sql-renderer.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('SqlRenderer', tsqlr,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

if postgres_dep.found() and get_option('postgres')
tsop = executable('test-server-operation-postgresql',
	['test-server-operation-postgresql.c',
	'test-cnc-utils.h',
	'test-cnc-utils.c',
	'test-errors.h',
	'test-errors.c',
	'raw-ddl-creator.h',
	'raw-ddl-creator.c'
	] ,
	c_args: [
		'-include',
		gda_top_build + '/config.h',
		'-DCHECK_FILES="'+gda_top_src+'"',
		'-DTOP_BUILD_DIR="'+gda_top_build+'"'
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ServerOperationPostgreSQL', tsop,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
endif
tso = executable('test-server-operation-sqlite',
	['test-server-operation-sqlite.c'] ,
	c_args: [
		'-include',
		gda_top_build + '/config.h',
		'-DCHECK_FILES="'+gda_top_src+'"',
		'-DTOP_BUILD_DIR="'+gda_top_build+'"'
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ServerOperationSQLite', tso,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
tmp = executable('test-provider-meta',
	['test-provider-meta.c'] ,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
# test('ProviderMeta', tmp,
# 	env: [
# 		'GDA_TOP_SRC_DIR='+gda_top_src,
# 		'GDA_TOP_BUILD_DIR='+gda_top_build
# 		]
# 	)

subdir('data-models')
subdir('meta-store')
subdir('multi-threading')
subdir('parser')
subdir('providers')
subdir('value-holders')
subdir('db')
if enable_ui
subdir('ui')
endif

if get_option('flatpak')
gda_test_sources = [
	'gda-test.c'
	]

executable ('org.gnome.gda.Test', gda_test_sources,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: true,
	link_with: libgda
	)
endif
