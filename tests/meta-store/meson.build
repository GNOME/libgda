meta_store_common_sources = files([
	'common.c',
	'common.h'
	])

tchkmst = executable('check_meta_store_memory',
	['check_meta_store_memory.c'] + meta_store_common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DTOP_SRC_DIR="'+gda_top_src+'"',
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
test('MetaStoreMemory', tchkmst,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkmstsqlite = executable('check_meta_store_sqlite',
	['check_meta_store_sqlite.c'] + meta_store_common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DTOP_SRC_DIR="'+gda_top_src+'"',
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
test('MetaStoreSqlite', tchkmstsqlite,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkmstspg = executable('check_meta_store_postgresql',
	['check_meta_store_postgresql.c'] + meta_store_common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DTOP_SRC_DIR="'+gda_top_src+'"',
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
test('MetaStorePostgres', tchkmstspg,
	timeout: 200,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkmstsmysql = executable('check_meta_store_mysql',
	['check_meta_store_mysql.c'] + meta_store_common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DTOP_SRC_DIR="'+gda_top_src+'"',
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
test('MetaStoreMySQL', tchkmstsmysql,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
