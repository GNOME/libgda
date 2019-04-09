inc_libgda_xslth = include_directories ('.')
inc_libgda_xslth_dep = declare_dependency (include_directories : inc_libgda_xslth)

gda_xslt_headers = files([
	'libgda-xslt.h'
	])

libgda_xslt_sources = files([
	'sql_backend.h',
	'sqlexslt.c',
	'sql_exslt_internal.c',
	'sql_backend.c'
	])
libgda_xslt_sources += gda_xslt_headers


libgda_xslt = library(
	'gda-xslt-'+project_api_version,
	libgda_xslt_sources,
	soversion : LT_VERSION+'.'+LT_AGE+'.'+LT_REVISION,
	version : LT_VERSION,
	dependencies : [libgda_dep,
			libxslt_dep,
			inc_libgda_xslth_dep,
			inc_rooth_dep,
			inc_libgdah_dep,
			inc_sqliteh_dep
			],
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h')
		],
	link_with: libgda,
	install : true
	)
