inc_libgda_sqliteh = include_directories ('.')
inc_libgda_sqliteh_dep = declare_dependency (include_directories : inc_libgda_sqliteh)

sqlite_xml_fnamesextf = []
foreach fn : sqlite_xml_fnames
	sqlite_xml_fnamesextf += [fn+'.xml.in']
endforeach

foreach cfn : sqlite_xml_fnames
	ocfn = configure_file(
		input: cfn+'.xml.in',
		output: cfn+'.xml',
		configuration: tconf
		)
	install_data(ocfn,
		install_dir: join_paths(get_option('datadir'), project_package)
		)
endforeach

foreach xt : sqlite_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml',
		type: 'xml',
		po_dir: 'po'
		)
endforeach

xml_res = []
foreach xr : sqlite_xml_fnames
	xml_res += custom_target(xr+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xr+'.xml.in',
		output: xr+'.raw.xml',
		)
endforeach

sqlite_resourcesc = custom_target('sqlite_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'providers','sqlite'),
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
		'--sourcedir='+join_paths(meson.source_root(),'providers','sqlite'),
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

libgda_sqlite_providerpc = configure_file(output : 'libgda-sqlite-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)
libgda_sqlite_provider = library ('gda-sqlite-'+project_api_version,
	libgda_sqlite_sources,
	dependencies: [
		libgda_dep,
		sqlite_dep,
		inc_libgda_sqliteh_dep,
		inc_sqliteh_dep,
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
