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
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

if sqlcipher_dep.found () and get_option('sqlcipher')

tchksqlcipher = executable('check_sqlcipher',
	['check_sqlcipher.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderSQLCipher', tchksqlcipher,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
endif

if postgres_dep.found () and get_option('postgres')

tchkpg = executable('check_postgres',
	['check_postgres.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderPostgreSQL', tchkpg,
	timeout: 300,
	is_parallel: false,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkpg = executable('check_postgresql_ddl',
	['check_postgresql_ddl.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderPostgreSQLDDL', tchkpg,
	timeout: 300,
	is_parallel: false,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkpg_mu1 = executable('check_postgres_meta_partial_1',
	['check_postgres_meta_partial-1.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderMetaPartialUpdate1PostgreSQL', tchkpg_mu1,
	timeout: 300,
	is_parallel: false,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkpg_mu2 = executable('check_postgres_meta_partial_2',
	['check_postgres_meta_partial-2.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderMetaPartialUpdate2PostgreSQL', tchkpg_mu2,
	timeout: 600,
	is_parallel: false,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

endif

if mysql_dep.found () and get_option('mysql')
tchkmysql = executable('check_mysql',
	['check_mysql.c']+providers_common_sources+common_sources+tests_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_SQL_FILES="'+gda_top_src+'"',
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
test('ProviderMySQL', tchkmysql,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
endif
