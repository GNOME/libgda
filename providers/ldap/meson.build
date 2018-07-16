
inc_libgda_ldaph = include_directories ('.')
inc_libgda_ldaph_dep = declare_dependency (include_directories : inc_libgda_ldaph)

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

ldap_xml_fnames = [
	'ldap_specs_auth',
	'ldap_specs_dsn',
	]

ldap_xml_fnamesext = []
foreach fn : ldap_xml_fnames
	ldap_xml_fnamesext += [fn+'.xml.in']
endforeach

ldap_xml_inf = files(ldap_xml_fnamesext)

foreach xf : ldap_xml_fnames
	install_data(xf+'.xml.in',
		rename: [
			xf+'.xml'
			],
		install_dir: join_paths(get_option('datadir'), project_package)
		)
endforeach

foreach xt : ldap_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

ldap_xml_res = []
foreach xrp : ldap_xml_fnames
	ldap_xml_res += custom_target(xrp+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xrp+'.xml.in',
		output: xrp+'.raw.xml',
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

libgda_ldap_provider = library ('gda-ldap-'+project_api_version,
	libgda_ldap_sources,
	dependencies: [
		libgda_dep,
		ldap_lib_dep,
		ldapber_dep,
		inc_sqliteh_dep,
		inc_libgdah_dep,
		inc_libgda_ldaph_dep
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