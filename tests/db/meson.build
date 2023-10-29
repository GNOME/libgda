inc_testsh = include_directories ('.')
inc_testsh_dep = declare_dependency (include_directories : inc_testsh)

db_sources = files([
	'check-db-catalog.c'
	])

tbc = executable('check-db-catalog',
	db_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbcatalog', tbc,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

dbtb = executable('check_db_base',
	'check-db-base.c',
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbbase', dbtb,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)



dbtc = executable('check_db_column',
	'check-db-column.c',
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbcolumn', dbtc,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

dbtv = executable('check_db_view',
	'check-db-view.c',
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbview', dbtv,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
dbfkey = executable('check_db_fkey',
	'check-db-fkey.c',
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbfkey', dbfkey,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
dbcreate = executable('check_db_create',
	'check-db-db-create.c',
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbcreate', dbcreate,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

if postgres_dep.found() and get_option('postgres')
db_catalog_pg_sources = files([
	'check-db-catalog-postgresql.c',
	'../test-cnc-utils.c',
	'../test-errors.c',
	'../raw-ddl-creator.c'
	])

tcpg = executable('check-db-catalog-postgresql',
	db_catalog_pg_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DCHECK_FILES="'+meson.project_source_root()+'"',
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
test('dbcatalog-postgresql', tcpg,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
endif