
subdir('virtual')

sqliteheaders = files(['gda-sqlite-provider.h'])
sqlitesources = files([
	'gda-sqlite-blob-op.c',
	'gda-sqlite-blob-op.h',
	'gda-sqlite-ddl.c',
	'gda-sqlite-ddl.h',
	'gda-sqlite-handler-bin.c',
	'gda-sqlite-handler-bin.h',
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
	'keywords_hash.h',
	'gda-symbols-util.h',
	'gda-symbols-util.c'
	])
sqlitesources += (sqliteheaders)

sqlite_cargs = [
		'-include',
		meson.build_root() + '/config.h',
		'-DPNAME="SQLite"', '-DCLASS_PREFIX="GdaSqlite"'
	]
sqlite_path = ''
if sqlite_dep.found ()
	sqlite_path = sqlite_dep.get_pkgconfig_variable('libdir')
	sqlite_cargs += ['-DHAVE_SQLITE']
endif

sqlite_cargs += ['-DSEARCH_LIB_PATH="'+sqlite_path+'"']
