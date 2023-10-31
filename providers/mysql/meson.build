inc_libgda_mysqlh = include_directories ('.')
inc_libgda_mysqlh_dep = declare_dependency (include_directories : inc_libgda_mysqlh)

gen_bin=find_program('gen_bin.py')

mysql_hander_binc = custom_target('mysql-hander-binc',
	command: [
		gen_bin,
		'-o=@OUTPUT@',
		'@INPUT@'
	],
	input: sqlitehanderbinc,
	output: 'gda-mysql-handler-bin.c'
	)

mysql_hander_binh = custom_target('mysql-hander-binh',
	command: [
		gen_bin,
		'-o=@OUTPUT@',
		'@INPUT@'
	],
	input: sqlitehanderbinh,
	output: 'gda-mysql-handler-bin.h'
	)

libgda_mysql_sources = files([
	'gda-mysql-blob-op.c',
	'gda-mysql-blob-op.h',
	'gda-mysql-ddl.c',
	'gda-mysql-ddl.h',
	'gda-mysql-handler-boolean.c',
	'gda-mysql-handler-boolean.h',
	'gda-mysql-provider.c',
	'gda-mysql-provider.h',
	'gda-mysql-recordset.c',
	'gda-mysql-recordset.h',
	'gda-mysql-util.c',
	'gda-mysql-util.h',
	'gda-mysql-pstmt.c',
	'gda-mysql-pstmt.h',
	'gda-mysql.h',
	'libmain.c'
	])

libgda_mysql_sources += mysql_hander_binc
libgda_mysql_sources += mysql_hander_binh

mysql_xml_fnames = [
	'mysql_specs_add_column',
	'mysql_specs_comment_column',
	'mysql_specs_comment_table',
	'mysql_specs_create_db',
	'mysql_specs_create_index',
	'mysql_specs_create_table',
	'mysql_specs_create_view',
	'mysql_specs_drop_column',
	'mysql_specs_drop_db',
	'mysql_specs_drop_index',
	'mysql_specs_drop_table',
	'mysql_specs_drop_view',
	'mysql_specs_dsn',
	'mysql_specs_rename_table',
	'mysql_specs_rename_column'
	]

mysql_xml_fnamesext = []
foreach fn : mysql_xml_fnames
	mysql_xml_fnamesext += [fn+'.xml.in']
endforeach

mysql_xml_inf = files(mysql_xml_fnamesext)

mysql_xml_res = []
foreach xt : mysql_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.project_build_root(),'po')
		)
	mysql_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

mysql_resourcesc = custom_target('mysql_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'mysql.gresource.xml',
	output: 'mysql_resource.c',
	depends: mysql_xml_res,
	install: false
	)

mysql_resourcesh = custom_target('mysql_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'mysql.gresource.xml',
	output: 'mysql_resource.h',
	depends: mysql_xml_res,
	install: false
	)
mysql_resources = [
	mysql_resourcesc,
	mysql_resourcesh
	]

libgda_mysql_sources += mysql_resources

libgda_mysql_sources += libgda_reusable_mysql_sources

mysql_args += [
	'-include',
	join_paths(gda_top_build, 'config.h'),
	'-DCLASS_PREFIX="GdaMySQL"'
	]

mysql_args += c_args

link_args = [ '-Wl,--export-dynamic' ]
host_os = host_machine.system().to_lower()
if host_os.contains('darwin') or host_os.contains('machten') or host_os.contains('rhapsody')
  link_args = [ '-Wl,-export_dynamic' ]
endif

libgda_mysql_provider = library ('gda-mysql-'+project_api_version,
	libgda_mysql_sources,
	dependencies: [
		libgda_dep,
		mysql_dep,
		mysql_reusable_dep,
		inc_libgdah_dep,
		inc_libgda_mysqlh_dep,
		inc_sqliteh_dep,
		inc_mysqlh_dep
		],
	c_args: mysql_args,
	link_args:link_args,
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)

libgda_mysql_providerpc = configure_file(
			input:  'libgda-mysql-'+project_api_version+'.pc.in',
			output: 'libgda-mysql-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)
