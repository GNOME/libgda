inc_libgda_sqliteh = include_directories ('.')
inc_libgda_sqliteh_dep = declare_dependency (include_directories : inc_libgda_sqliteh)

xml_fnames = [
	'sqlite_specs_dsn',
	'sqlite_specs_create_db',
	'sqlite_specs_drop_db',
	'sqlite_specs_create_table',
	'sqlite_specs_drop_table',
	'sqlite_specs_create_index',
	'sqlite_specs_drop_index',
	'sqlite_specs_rename_table',
	'sqlite_specs_add_column',
	'sqlite_specs_create_view',
	'sqlite_specs_drop_view'
	]
xml_fnamesext = []
foreach fn : xml_fnames
	xml_fnamesext += [fn+'.xml.in']
endforeach

xml_inf = files(xml_fnamesext)

install_data(xml_fnamesext,
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
foreach xt : xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

foreach xr : xml_fnames
	tc = configuration_data()
	configure_file(
		copy: true,
		input: xr+'.xml.in',
		output: xr+'.raw.xml',
		configuration: tc
		)
endforeach

sqlite_resources = gnome_module.compile_resources('sqlite_resources', 'sqlite.gresource.xml',
	source_dir: meson.current_build_dir()
	)

libgda_sqlite_sources = files([
	'libmain.c'
	])

conf = configuration_data()
conf.set_quoted('GDA_ABI_MAJOR_VERSION', MAJOR_VERSION)
conf.set_quoted('GDA_ABI_MINOR_VERSION', MINOR_VERSION)
conf.set_quoted('VERSION', project_version)

libgda_sqlite_providerpc = configure_file(output : 'libgda-sqlite-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)
libgda_sqlite_provider = library ('libgda-sqlite',
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
