
inc_libgda_capih = include_directories ('.')
inc_libgda_capih_dep = declare_dependency (include_directories : inc_libgda_capih)

capi_xml_fnames = [
	'capi_specs_dsn',
	'capi_specs_create_table',
	]

capi_xml_res = []
foreach xt : capi_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml',
		type: 'xml',
		po_dir: join_paths(meson.project_build_root(),'po')
		)
	capi_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)
endforeach

capi_resourcesc = custom_target('capi_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.project_source_root(),'providers','capi'),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'capi.gresource.xml',
	output: 'capi_resource.c',
	depends: capi_xml_res,
	install: false
	)

capi_resourcesh = custom_target('capi_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.project_source_root(),'providers','capi'),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'capi.gresource.xml',
	output: 'capi_resource.h',
	depends: capi_xml_res,
	install: false
	)
capi_resources = []
capi_resources += capi_resourcesc
capi_resources += capi_resourcesh


libgda_capi_sources = files([
	'gda-capi-blob-op.c',
	'gda-capi-blob-op.h',
	'gda-capi-ddl.c',
	'gda-capi-ddl.h',
	'gda-capi-parser.c',
	'gda-capi-parser.h',
	'gda-capi-provider.c',
	'gda-capi-provider.h',
	'gda-capi-pstmt.h',
	'gda-capi-pstmt.c',
	'gda-capi-meta.c',
	'gda-capi-meta.h',
	'gda-capi-recordset.c',
	'gda-capi-recordset.h',
	'gda-capi.h',
	'libmain.c',
	])
libgda_capi_sources += capi_resources

libgda_sqlite_providerpc = configure_file(
			input:  'libgda-capi-'+project_api_version+'.pc.in',
			output: 'libgda-capi-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

capi_keywordshash = custom_target('keywords_hash_capi',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords.list',
		output: 'keywords_hash.code'
		)

capi_keywordshash_dep = declare_dependency (sources: capi_keywordshash)

capi_lemon_parserc = custom_target('capi_lemon-parser_c',
	command : [lemon_exe, '-d', '-h',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.c'],
	install : false
	)


capi_lemon_parserh = custom_target('capi_lemon-parser_h',
	command : [lemon_exe, '-d', '-z',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.h'],
	install : false
	)

capi_lemon_parserh_dep = declare_dependency(sources: capi_lemon_parserh)

capi_gendef = executable('capi_gen_def', 'gen_def.c',
	c_args: [
		'-DIMPOSED_HEADER="'+join_paths (gda_top_build, 'libgda', 'sql-parser','token_types.h')+'"',
		'-DPARSER_HEADER_PATH="'+join_paths(meson.current_build_dir(),'parser.h')+'"'
		],
	dependencies: [capi_lemon_parserh_dep],
	install: false,
	)

capi_token_types = custom_target('capi_token_types',
	command: [capi_gendef],
	capture: true,
	input: [capi_lemon_parserh],
	output: 'capi_token_types.h',
	install: false,
	)


libgda_capi_sources += capi_lemon_parserc
libgda_capi_sources += capi_lemon_parserh
libgda_capi_sources += capi_token_types

libgda_capi_provider_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		]

libgda_capi_provider_cargs += c_args

libgda_capi_provider = library ('gda-capi',
	libgda_capi_sources,
	dependencies: [
		libgda_dep,
		sqlite_dep,
		capi_keywordshash_dep,
		inc_libgda_capih_dep,
		inc_sqliteh_dep,
		],
	c_args: libgda_capi_provider_cargs,
	link_args: [
		'-Wl,--export-dynamic',
		],
	link_with: libgda,
	install: false,
	)
