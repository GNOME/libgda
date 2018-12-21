inc_testsh = include_directories ('.')
inc_testsh_dep = declare_dependency (include_directories : inc_testsh)

db_sources = files([
	'check-db-catalog.c'
	])

tbc = executable('check-db-catalog',
	db_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

dbtb = executable('check_db_base',
	'check-db-base.c',
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)



dbtc = executable('check_db_column',
	'check-db-column.c',
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

dbtv = executable('check_db_view',
	'check-db-view.c',
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)
dbfkey = executable('check_db_fkey',
	'check-db-fkey.c',
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)
dbcreate = executable('check_db_create',
	'check-db-db-create.c',
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

db_catalog_pg_sources = files([
	'check-db-catalog-postgresql.c'
	])

tcpg = executable('check-db-catalog-postgresql',
	db_catalog_pg_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"',
		ci_args
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
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)
