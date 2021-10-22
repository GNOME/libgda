inc_libgda_sqliteh = include_directories ('.')
inc_libgda_sqliteh_dep = declare_dependency (include_directories : inc_libgda_sqliteh)

xml_res = []
foreach xt : sqlite_xml_fnames
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

sqlite_resourcesc = custom_target('sqlite_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+meson.current_source_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'sqlite.gresource.xml',
	output: 'sqlite_resource.c',
	depends: xml_res,
	install: false
	)

sqlite_resourcesh = custom_target('sqlite_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+meson.current_source_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'sqlite.gresource.xml',
	output: 'sqlite_resource.h',
	depends: xml_res,
	install: false
	)
sqlite_resources = []
sqlite_resources += sqlite_resourcesc
sqlite_resources += sqlite_resourcesh

libgda_sqlite_sources = files([
	'libmain.c'
	])
libgda_sqlite_sources += sqlite_resources

libgda_sqlite_providerpc_name = 'libgda-sqlite-'+project_api_version+'.pc'
libgda_sqlite_providerpc = configure_file(
			input : libgda_sqlite_providerpc_name+'.in',
			output : libgda_sqlite_providerpc_name,
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_sqlite_provider_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-I'+join_paths(get_option ('prefix'), get_option('includedir'))
		]
libgda_sqlite_provider_cargs += c_args

libgda_sqlite_provider = library ('gda-sqlite-'+project_api_version,
	libgda_sqlite_sources,
	dependencies: [
		libgda_dep,
		sqlite_dep,
		inc_libgda_sqliteh_dep,
		inc_sqliteh_dep,
		inc_libgdah_dep
		],
	c_args: libgda_sqlite_provider_cargs,
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
