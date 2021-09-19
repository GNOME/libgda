
jdbc_doc_ignore = files([
	'jni-wrapper.h',
	'gda-jdbc-recordset.h',
	'gda-jdbc-blob-op.h',
	'gda-jdbc-provider.h',
	'gda-jdbc-pstmt.h'
])

jni_filenames = [
	'GdaJConnection',
	'GdaJMeta',
	'GdaJProvider',
	'GdaJPStmt',
	'GdaJResultSet',
	'GdaJResultSetInfos',
	'GdaJColumnInfos',
	'GdaJBlobOp',
	'GdaJValue',
	'GdaInputStream'
]

jni_sources_fnamesext = []
jni_headers_fnamesext = []
jni_classes_fnamesext = []
jni_classes_jar = []
foreach fn : jni_filenames
	jni_sources_fnamesext += [fn +'.c']
	jni_headers_fnamesext += [fn +'.h']
	jni_classes_fnamesext += [fn +'.class']
	jni_classes_jar += ['-C', meson.current_build_dir(), fn +'.class']
endforeach

jni_headers = jni_headers_fnamesext
jni_sources = files(jni_sources_fnamesext)

jdbc_xml_fnames = [
	'jdbc_specs_dsn',
	'jdbc_specs_com.microsoft.sqlserver.jdbc.SQLServerDriver_dsn',
]

jdbc_xml_res = []
foreach xt : jdbc_xml_fnames
	i18n.merge_file(
		input: xt + '.xml.in',
		output: xt + '.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
	)
	jdbc_xml_res += custom_target(xt + '.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt + '.xml.in',
		output: xt + '.raw.xml',
	)
endforeach

jdbc_resourcesc = custom_target('jdbc_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
	],
	input: 'jdbc.gresource.xml',
	output: 'jdbc_resource.c',
	depends: jdbc_xml_res,
	install: false
	)

jdbc_resourcesh = custom_target('jdbc_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
	],
	input: 'jdbc.gresource.xml',
	output: 'jdbc_resource.h',
	depends: jdbc_xml_res,
	install: false
	)

resources = []
resources += jdbc_resourcesh
resources += jdbc_resourcesc

libgda_jdbc_sources = files([
	'gda-jdbc-blob-op.c',
	'gda-jdbc-blob-op.h',
	'gda-jdbc-ddl.c',
	'gda-jdbc-ddl.h',
	'gda-jdbc-provider.c',
	'gda-jdbc-provider.h',
	'gda-jdbc-pstmt.h',
	'gda-jdbc-pstmt.c',
	'gda-jdbc-meta.c',
	'gda-jdbc-meta.h',
	'gda-jdbc-recordset.c',
	'gda-jdbc-recordset.h',
	'gda-jdbc-util.c',
	'gda-jdbc-util.h',
	'gda-jdbc.h',
	'libmain.c',
	'jni-globals.h',
	'jni-wrapper.h',
	'jni-wrapper.c'
])

libgda_jdbc_sources += jni_sources
libgda_jdbc_sources += resources

# generate jdbc provider jar
jdbcprov_sources = files([
	'provider.java',
	'meta.java',
	'derby.java',
	'h2.java',
	'sqlserver.java'
])

javac = find_program('javac')
jni_jdbc_sources = custom_target('jni-sources',
	command: [javac,
		'-classpath', meson.current_build_dir(),
		'-d', meson.current_build_dir(),
		'@INPUT@'
	],
	input: jdbcprov_sources,
	output: jni_classes_fnamesext,
	install: false
	)
jni_jdbc_headers = custom_target('jni-headers',
	command: [javac,
		'-classpath', meson.current_build_dir(),
		'-h', meson.current_build_dir(),
		'-d', meson.current_build_dir(),
		'@INPUT@'
	],
	input: jdbcprov_sources,
	output: jni_headers_fnamesext,
	install: false
	)

libgda_jdbc_sources += jni_jdbc_headers

jdbc_manifest = files([
	'MANIFEST.MF'
])

jar = find_program('jar')

libgda_jdbc_provider_jar = custom_target(
	'gdaprovider-' + project_api_version + '.jar',
	command: [jar,
		'--create', '--file', '@OUTPUT@',
		'--manifest=@INPUT@',
		jni_classes_jar
	],
	input: jdbc_manifest,
    	output: 'gdaprovider-'+project_api_version+'.jar',
    	depends: jni_jdbc_headers,
	install : true,
  	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
)

jdbc_args = [
	'-include',
	join_paths(gda_top_build, 'config.h'),
	'-DCLASS_PREFIX="GdaJdbc"'
]
jdbc_args += c_args

link_args  = [ '-Wl', '--export-dynamic', '-module', '-avoid-version' ]
host_os = host_machine.system().to_lower()
if host_os.contains('darwin') or host_os.contains('machten') or host_os.contains('rhapsody')
  link_args = [ '-Wl', '-export_dynamic', '-module', '-avoid-version' ]
endif

# libgda_jdbc_provider = library ('gda-jdbc-' + project_api_version,
# 	libgda_jdbc_sources,
# 	dependencies: [
# 		libgda_dep,
# 		inc_libgdah_dep,
# 		inc_sqliteh_dep,
# 	],
# 	c_args: jdbc_args,
# 	link_args: link_args,
# 	link_with: libgda,
# 	install: true,
# 	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
# )

libgda_jdbc_provider_pc = configure_file(
	input:  'libgda-jdbc-'+project_api_version+'.pc.in',
	output: 'libgda-jdbc-'+project_api_version+'.pc',
	configuration : conf,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
)
