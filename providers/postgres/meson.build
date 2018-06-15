
inc_libgda_postgresh = include_directories ('.')
inc_libgda_postgresh_dep = declare_dependency (include_directories : inc_libgda_postgresh)

postgres_xml_fnames = [
	'postgres_specs_add_column',
	'postgres_specs_create_db',
	'postgres_specs_create_index',
	'postgres_specs_create_role',
	'postgres_specs_create_table',
	'postgres_specs_create_user',
	'postgres_specs_create_view',
	'postgres_specs_drop_column',
	'postgres_specs_drop_db',
	'postgres_specs_drop_index',
	'postgres_specs_drop_role',
	'postgres_specs_drop_table',
	'postgres_specs_drop_user',
	'postgres_specs_drop_view',
	'postgres_specs_dsn',
	'postgres_specs_rename_table'
	]
postgres_xml_fnamesext = []
foreach fn : postgres_xml_fnames
	postgres_xml_fnamesext += [fn+'.xml.in']
endforeach

postgres_xml_inf = files(postgres_xml_fnamesext)

foreach xf : postgres_xml_fnames
	install_data(xf+'.xml.in',
		rename: [
			xf+'.xml'
			],
		install_dir: join_paths(get_option('datadir'), project_package)
		)
endforeach

foreach xt : postgres_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

foreach xr : postgres_xml_fnames
	tc = configuration_data()
	configure_file(
		copy: true,
		input: xr+'.xml.in',
		output: xr+'.raw.xml',
		configuration: tc
		)
endforeach

postgres_resources = gnome_module.compile_resources('postgres_resources', 'postgres.gresource.xml',
	source_dir: meson.current_build_dir()
	)

libgda_postgres_sources = files([
	'gda-postgres-blob-op.c',
	'gda-postgres-blob-op.h',
	'gda-postgres-ddl.c',
	'gda-postgres-ddl.h',
	'gda-postgres-handler-bin.c',
	'gda-postgres-handler-bin.h',
	'gda-postgres-provider.c',
	'gda-postgres-provider.h',
	'gda-postgres-pstmt.c',
	'gda-postgres-pstmt.h',
	'gda-postgres-recordset.c',
	'gda-postgres-recordset.h',
	'gda-postgres-util.c',
	'gda-postgres-util.h',
	'gda-postgres.h',
	'libmain.c'
	])

conf = configuration_data()
conf.set_quoted('GDA_ABI_MAJOR_VERSION', MAJOR_VERSION)
conf.set_quoted('GDA_ABI_MINOR_VERSION', MINOR_VERSION)
conf.set_quoted('VERSION', project_version)

libgda_sqlite_providerpc = configure_file(
			input:  'libgda-postgres-'+project_api_version+'.pc.in',
			output: 'libgda-postgres-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_postgres_sources += libgda_pg_sources

libgda_postgres_provider = library ('gda-postgres-'+project_api_version,
	libgda_postgres_sources,
	dependencies: [
		libgda_dep,
		postgres_dep,
		pg_reusable_dep,
		inc_postgresh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_args: [
		'-export-dynamic',
		],
	link_with: libgda,
	install: false,
	)
