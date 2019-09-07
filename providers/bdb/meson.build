inc_libgda_bdbh = include_directories ('.')
inc_libgda_bdbh_dep = declare_dependency (include_directories : inc_libgda_bdbh)

libgda_bdb_sources = files([
	'gda-bdb.h',
	'gda-bdb-provider.c',
	'gda-bdb-provider.h',
	'libmain.c'
	])

bdb_xml_fnames = [
	'bdb_specs_dsn',
	]

bdb_xml_fnamesext = []
foreach fn : bdb_xml_fnames
	bdb_xml_fnamesext += [fn+'.xml.in']
endforeach

bdb_xml_inf = files(bdb_xml_fnamesext)

bdb_xml_res = []
foreach xt : bdb_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
	bdb_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

bdb_resourcesc = custom_target('bdb_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'bdb.gresource.xml',
	output: 'bdb_resource.c',
	depends: bdb_xml_res,
	install: false
	)

bdb_resourcesh = custom_target('bdb_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'bdb.gresource.xml',
	output: 'bdb_resource.h',
	depends: bdb_xml_res,
	install: false
	)
bdb_resources = [
	bdb_resourcesc,
	bdb_resourcesh
	]

libgda_bdb_sources += bdb_resources

libgda_bdb_provider = library ('gda-bdb-'+project_api_version,
	libgda_bdb_sources,
	dependencies: [
		libgda_dep,
		bdb_lib,
		inc_libgdah_dep,
		inc_libgda_bdbh_dep,
		inc_sqliteh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		lbdb_cargs
		],
	link_args: [
		'-Wl,--export-dynamic',
		],
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)

# Tests

gda_bdb_test_sources = files (['gda-bdb-test.c'])

bdbt = executable ('gda_bdb_test',
	gda_bdb_test_sources,
	dependencies: [
		libgda_dep,
		bdb_lib,
		inc_libgdah_dep,
		inc_libgda_bdbh_dep,
		inc_sqliteh_dep
		],
	c_args: [
		lbdb_cargs,
		'-DBDB_CURRENT_BUILDDIR="'+meson.current_build_dir()+'"'
		],
	link_with: [
		libgda
		]
	)

test('BDB', bdbt)
