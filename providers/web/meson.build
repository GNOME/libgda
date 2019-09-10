

inc_gda_webh = include_directories ('.')
inc_gda_webh_dep = declare_dependency (include_directories : inc_gda_webh)

gda_web_sources = files([
	'gda-web-blob-op.c',
	'gda-web-blob-op.h',
	'gda-web-ddl.c',
	'gda-web-ddl.h',
	'gda-web-provider.c',
	'gda-web-provider.h',
	'gda-web-pstmt.h',
	'gda-web-pstmt.c',
	'gda-web-meta.c',
	'gda-web-meta.h',
	'gda-web-recordset.c',
	'gda-web-recordset.h',
	'gda-web-util.h',
	'gda-web-util.c',
	'gda-web.h',
	'libmain.c'
	])

gda_web_xml_fnames = [
	'web_specs_dsn',
	'web_specs_auth'
	]

gda_web_xml_res = []
foreach xt : gda_web_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
	gda_web_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

gda_web_resourcesc = custom_target('gda_web_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'web.gresource.xml',
	output: 'gda_web_resource.c',
	depends: gda_web_xml_res,
	install: false
	)

gda_web_resourcesh = custom_target('gda_web_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'web.gresource.xml',
	output: 'gda_web_resource.h',
	depends: gda_web_xml_res,
	install: false
	)
gda_web_resources = []
gda_web_resources += gda_web_resourcesc
gda_web_resources += gda_web_resourcesh

libgda_web_providerpc = configure_file(
			input:  'libgda-web-'+project_api_version+'.pc.in',
			output: 'libgda-web-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_web_sources = []
libgda_web_sources += gda_web_sources
libgda_web_sources += gda_web_resources
libgda_web_sources += libreuseable_sources

libgda_web_provider = library ('gda-web-'+project_api_version,
	libgda_web_sources,
	dependencies: [
		libgda_dep,
		soup_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_gda_webh_dep,
		],
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		],
	link_args: [
		'-Wl,--export-dynamic',
		],
	link_with: [
		libgda,
		libgda_mysql_provider,
		libgda_postgres_provider
		],
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
