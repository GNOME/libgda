mkkeywordhashf = files([
	'mkkeywordhash.c'
])

mkkeywordhash = executable ('mkkeywordhash',
	mkkeywordhashf,
	c_args: c_args
	)

subdir('virtual')

gen_emb_string = executable('gen_emb_string', 'gen_emb_string.c')

sqliteheaders = files(['gda-sqlite-provider.h'])
sqlitehanderbinc = files([
	'gda-sqlite-handler-bin.c',
	])
sqlitehanderbinh = files([
	'gda-sqlite-handler-bin.h'
	])
sqlitesources_provider = files([
	'gda-sqlite-blob-op.c',
	'gda-sqlite-blob-op.h',
	'gda-sqlite-ddl.c',
	'gda-sqlite-ddl.h',
	'gda-sqlite-handler-boolean.c',
	'gda-sqlite-handler-boolean.h',
	'gda-sqlite-meta.c',
	'gda-sqlite-meta.h',
	'gda-sqlite-provider.c',
	'gda-sqlite-pstmt.h',
	'gda-sqlite-pstmt.c',
	'gda-sqlite-recordset.c',
	'gda-sqlite-recordset.h',
	'gda-sqlite-util.c',
	'gda-sqlite-util.h',
	'gda-sqlite.h',
	'keywords_hash.h'
	])
sqlitesources = []
sqlitesources += sqlitesources_provider
sqlitesources += sqliteheaders
sqlitesources += sqlitehanderbinc
sqlitesources += sqlitehanderbinh

sqlite_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DPNAME="SQLite"', '-DCLASS_PREFIX="GdaSqlite"'
	]
sqlite_path = ''

sqlite_deps = []

sqlite_path = sqlite_dep.get_variable('libdir')
sqlite_cargs += ['-DHAVE_SQLITE']
sqlite_deps += sqlite_dep

sqlite_cargs += ['-DSEARCH_LIB_PATH="'+sqlite_path+'"']
