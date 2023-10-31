
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
	'postgres_specs_rename_table',
	'postgres_specs_rename_column'
	]

postgres_xml_res = []
foreach xt : postgres_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml',
		type: 'xml',
		po_dir: join_paths(meson.project_build_root(),'po')
		)
	postgres_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

postgres_resourcesc = custom_target('postgres_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'postgres.gresource.xml',
	output: 'postgres_resource.c',
	depends: postgres_xml_res,
	install: false
	)

postgres_resourcesh = custom_target('postgres_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'postgres.gresource.xml',
	output: 'postgres_resource.h',
	depends: postgres_xml_res,
	install: false
	)
postgres_resources = []
postgres_resources += postgres_resourcesc
postgres_resources += postgres_resourcesh

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
libgda_postgres_sources += postgres_resources

libgda_postgres_providerpc = configure_file(
			input:  'libgda-postgres-'+project_api_version+'.pc.in',
			output: 'libgda-postgres-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_postgres_sources += libgda_pg_sources

libgda_postgres_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		]

libgda_postgres_cargs += c_args

link_args = [ '-Wl,--export-dynamic' ]
host_os = host_machine.system().to_lower()
if host_os.contains('darwin') or host_os.contains('machten') or host_os.contains('rhapsody')
  link_args = [ '-Wl,-export_dynamic' ]
endif

libgda_postgres_provider = library ('gda-postgres-'+project_api_version,
	libgda_postgres_sources,
	dependencies: [
		libgda_dep,
		postgres_dep,
		pg_reusable_dep,
		inc_postgresh_dep
		],
	c_args: libgda_postgres_cargs,
	link_args: link_args,
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
