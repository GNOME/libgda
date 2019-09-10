
inc_libgda_ldaph = include_directories ('.')
inc_libgda_ldaph_dep = declare_dependency (include_directories : inc_libgda_ldaph)

ldaph_sources = files ([
	'gda-ldap-connection.h',
	'gda-data-model-ldap.h',
	'gda-tree-mgr-ldap.h'
	])
install_headers(ldaph_sources,
	install_dir: join_paths(get_option('includedir'),project_package,'providers','ldap')
	)

ldapc_sources = files ([
	'gda-ldap-connection.c',
	'gda-data-model-ldap.c',
	'gda-tree-mgr-ldap.c'
	])

libgda_ldap_sources = files([
	'gdaprov-data-model-ldap.c',
	'gdaprov-data-model-ldap.h',
	'gda-ldap-provider.c',
	'gda-ldap-provider.h',
	'gda-ldap-util.c',
	'gda-ldap-util.h',
	'gda-ldap.h',
	'libmain.c'
	])

libgda_ldap_sources += ldaph_sources
libgda_ldap_sources += ldapc_sources

ldap_xml_fnames = [
	'ldap_specs_auth',
	'ldap_specs_dsn',
	]

ldap_xml_fnamesext = []
foreach fn : ldap_xml_fnames
	ldap_xml_fnamesext += [fn+'.xml.in']
endforeach

ldap_xml_inf = files(ldap_xml_fnamesext)

ldap_xml_res = []
foreach xt : ldap_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
	ldap_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

ldap_resourcesc = custom_target('ldap_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'ldap.gresource.xml',
	output: 'ldap_resource.c',
	depends: ldap_xml_res,
	install: false
	)

ldap_resourcesh = custom_target('ldap_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'ldap.gresource.xml',
	output: 'ldap_resource.h',
	depends: ldap_xml_res,
	install: false
	)
ldap_resources = [
	ldap_resourcesc,
	ldap_resourcesh
	]

libgda_ldap_sources += ldap_resources

libgda_ldap_providerpc = configure_file(
			input:  'libgda-ldap-'+project_api_version+'.pc.in',
			output: 'libgda-ldap-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_ldap_provider_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		]
libgda_ldap_provider_cargs += c_args

libldap_provider = library ('gda-ldap-'+project_api_version,
	libgda_ldap_sources,
	dependencies: [
		libgda_dep,
		ldap_lib_dep,
		ldapber_dep,
		inc_sqliteh_dep,
		inc_libgdah_dep,
		inc_libgda_ldaph_dep
		],
	c_args: libgda_ldap_provider_cargs,
	link_args: [
		'-Wl,--export-dynamic',
		],
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)

libldap_provider_dep = declare_dependency(link_with: libldap_provider)

