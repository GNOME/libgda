providers_common_sources = files([
	'prov-test-common.h',
	'prov-test-common.c',
	'prov-test-util.h',
	'prov-test-util.c'
	])

tchksqlite = executable('check_sqlite',
	['check_sqlite.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_SQL_FILES="'+meson.source_root()+'"',
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('ProviderSQLite', tchksqlite,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)