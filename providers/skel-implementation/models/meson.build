
inc_libgda_modelsh = include_directories ('.')
inc_libgda_modelsh_dep = declare_dependency (include_directories : inc_libgda_modelsh)

models_xml_fnames = [
	'models_specs_dsn',
	]
models_xml_res = []
foreach xt : models_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(gda_top_build,'po')
		)
	models_xml_res += custom_target(xt+'.raw.xml',
		command: [raw_spec,
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: xt+'.xml.in',
		output: xt+'.raw.xml',
		)

	# osf = configure_file(
	# 	input: xt+'.xml.in',
	# 	output: xt+'.xml',
	# 	configuration: tconf
	# 	)
	# install_data(osf,
	# 	install_dir: join_paths(get_option('datadir'), project_package)
	# 	)
endforeach

models_resources = gnome_module.compile_resources('models_resources', 'models.gresource.xml',
	source_dir: meson.current_build_dir(),
	dependencies: models_xml_res
	)

libgda_models_sources = files([
	'gda-models-provider.c',
	'gda-models-provider.h',
	'gda-models.h',
	'libmain.c'
	])

libgda_sqlite_providerpc = configure_file(
			input:  'libgda-models-'+project_api_version+'.pc.in',
			output: 'libgda-models-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)

libgda_models_provider_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		]
libgda_models_provider_cargs += c_args

libgda_models_provider = library ('libgda-models',
	libgda_models_sources,
	dependencies: [
		libgda_dep,
		sqlite_dep,
		inc_libgda_modelsh_dep,
		inc_sqliteh_dep,
		],
	c_args: libgda_models_provider_cargs,
	link_with: libgda,
	install: false,
	)
