inc_libgda_sqliteh = include_directories ('.')
inc_libgda_sqliteh_dep = declare_dependency (include_directories : inc_libgda_sqliteh)

sqlite_xml_fnamesextf = []
foreach fn : sqlite_xml_fnames
	sqlite_xml_fnamesextf += [fn+'.xml.in']
endforeach

install_data(sqlite_xml_fnamesextf,
	rename: [
		'sqlite_specs_dsn.xml',
		'sqlite_specs_create_db.xml',
		'sqlite_specs_drop_db.xml',
		'sqlite_specs_create_table.xml',
		'sqlite_specs_drop_table.xml',
		'sqlite_specs_create_index.xml',
		'sqlite_specs_drop_index.xml',
		'sqlite_specs_rename_table.xml',
		'sqlite_specs_add_column.xml',
		'sqlite_specs_create_view.xml',
		'sqlite_specs_drop_view.xml'
		],
	install_dir: join_paths(get_option('datadir'), project_package)
	)
foreach xt : sqlite_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

xml_res = []
str='@0@'.format('\\\\')
single_quote = 'contains a \' character'
foreach xr : sqlite_xml_fnames
# sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'>
	xml_res += custom_target(xr+'.raw.xml',
		command: [sed,
			'-e', 's/_([a-zA-Z0-9]*)=/1=/g',
			],
		input: xr+'.xml.in',
		output: xr+'.raw.xml',
		capture: true,
		)
endforeach

sqlite_resourcesc = custom_target('sqlite_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
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

conf = configuration_data()
conf.set_quoted('GDA_ABI_MAJOR_VERSION', MAJOR_VERSION)
conf.set_quoted('GDA_ABI_MINOR_VERSION', MINOR_VERSION)
conf.set_quoted('VERSION', project_version)

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
