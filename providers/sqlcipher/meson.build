inc_libgda_sqlcipherh = include_directories ('.')
inc_libgda_sqlcipherh_dep = declare_dependency (include_directories : inc_libgda_sqlcipherh)

sqlcipher_xml_fnames = [
	'sqlcipher_specs_add_column',
	'sqlcipher_specs_auth',
	'sqlcipher_specs_create_db',
	'sqlcipher_specs_create_index',
	'sqlcipher_specs_create_table',
	'sqlcipher_specs_create_view',
	'sqlcipher_specs_drop_db',
	'sqlcipher_specs_drop_index',
	'sqlcipher_specs_drop_table',
	'sqlcipher_specs_drop_view',
	'sqlcipher_specs_dsn',
	'sqlcipher_specs_rename_table'
	]

xml_res = []
foreach xt : sqlcipher_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml',
		type: 'xml',
		po_dir: 'po',
		install_dir: join_paths(get_option('datadir'),project_package)
		)
	xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

sqlcipher_resourcesc = custom_target('sqlcipher_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'providers','sqlcipher'),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'sqlcipher.gresource.xml',
	output: 'sqlcipher_resource.c',
	depends: xml_res,
	install: false
	)

sqlcipher_resourcesh = custom_target('sqlcipher_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'providers','sqlcipher'),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'sqlcipher.gresource.xml',
	output: 'sqlcipher_resource.h',
	depends: xml_res,
	install: false
	)
sqlcipher_resources = []
sqlcipher_resources += sqlcipher_resourcesc
sqlcipher_resources += sqlcipher_resourcesh

libgda_sqlcipher_sources = files([
	'libmain.c',
	'gda-sqlcipher-provider.c'
	])
libgda_sqlcipher_sources += sqlcipher_resources

sqlcipherheaders = files(['gda-sqlcipher-provider.h'])
libgda_sqlcipher_sources += sqlcipherheaders

install_headers (sqlcipherheaders, subdir: join_paths(project_package, 'providers', 'sqlcipher'))

sqlcipher_path = sqlcipher_dep.get_pkgconfig_variable('libdir')
sqlcipher_cargs = ['-DHAVE_SQLITE']
sqlcipher_deps = [sqlcipher_dep]

sqlcipher_cargs += ['-DSEARCH_LIB_PATH="'+sqlcipher_path+'"']

sqlcipher_cargs += [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DSQLITE_HAS_CODEC',
		'-DSQLITE_API=',
		'-DSQLITE_PRIVATE=',
		'-DSQLITE_ENABLE_COLUMN_METADATA',
		'-DSQLITE_THREADSAFE=1'
		]
sqlcipher_cargs += c_args


libgda_sqlcipher_providerpc_name = 'libgda-sqlcipher-'+project_api_version+'.pc'
libgda_sqlcipher_providerpc = configure_file(
			input : libgda_sqlcipher_providerpc_name+'.in',
			output : libgda_sqlcipher_providerpc_name,
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_sqlcipher_provider = library ('gda-sqlcipher-'+project_api_version,
	libgda_sqlcipher_sources,
	dependencies: [
		libgda_dep,
		sqlcipher_deps,
		inc_libgda_sqlcipherh_dep,
		inc_sqliteh_dep,
		inc_libgdah_dep,
		inc_rooth_dep
		],
	c_args: sqlcipher_cargs,
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
