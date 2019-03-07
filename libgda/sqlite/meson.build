mkkeywordhashf = files([
	'mkkeywordhash.c'
])

mkkeywordhash = executable ('mkkeywordhash', mkkeywordhashf)

subdir('virtual')

gen_emb_string = executable('gen_emb_string', 'gen_emb_string.c')

xml_embeddedh = custom_target('xml_embeddedh',
	command: [gen_emb_string, '@INPUT@'],
	input: sqlite_xml_inf,
	output: 'xml_embedded.h',
	install: false,
	capture: true
	)

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
sqlitesources += xml_embeddedh
sqlitesources += sqlitehanderbinc
sqlitesources += sqlitehanderbinh

sqlite_cargs = [
		'-include',
		meson.build_root() + '/config.h',
		'-DPNAME="SQLite"', '-DCLASS_PREFIX="GdaSqlite"'
	]
sqlite_path = ''

sqlite_deps = []

sqlite_path = sqlite_dep.get_pkgconfig_variable('libdir')
sqlite_cargs += ['-DHAVE_SQLITE']
sqlite_deps += sqlite_dep

sqlite_cargs += ['-DSEARCH_LIB_PATH="'+sqlite_path+'"']
