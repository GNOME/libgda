
inc_libgda_modelsh = include_directories ('.')
inc_libgda_modelsh_dep = declare_dependency (include_directories : inc_libgda_modelsh)

models_xml_fnames = [
	'models_specs_dsn',
	]
models_xml_fnamesext = []
foreach fn : models_xml_fnames
	models_xml_fnamesext += [fn+'.xml.in']
endforeach

models_xml_inf = files(models_xml_fnamesext)

install_data(models_xml_fnamesext,
	rename: [
		'sqlite_specs_dsn.xml',
		],
	install_dir: join_paths(get_option('datadir'), project_package)
	)
foreach xt : models_xml_fnames
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

foreach xr : models_xml_fnames
	tc = configuration_data()
	configure_file(
		copy: true,
		input: xr+'.xml.in',
		output: xr+'.raw.xml',
		configuration: tc
		)
endforeach

models_resources = gnome_module.compile_resources('models_resources', 'models.gresource.xml',
	source_dir: meson.current_build_dir()
	)

libgda_models_sources = files([
	'gda-models-provider.c',
	'gda-models-provider.h',
	'gda-models.h',
	'models.gresources.c',
	'libmain.c'
	])

conf = configuration_data()
conf.set_quoted('GDA_ABI_MAJOR_VERSION', MAJOR_VERSION)
conf.set_quoted('GDA_ABI_MINOR_VERSION', MINOR_VERSION)
conf.set_quoted('VERSION', project_version)

libgda_sqlite_providerpc = configure_file(
			input:  'libgda-models-'+project_api_version+'.pc.in',
			output: 'libgda-models-'+project_api_version+'.pc',
			configuration : conf,
			install_dir: join_paths(get_option('libdir'),'pkgconfig')
			)
libgda_models_provider = library ('libgda-models',
	libgda_models_sources,
	dependencies: [
		libgda_dep,
		sqlite_dep,
		inc_libgda_modelsh_dep,
		inc_sqliteh_dep,
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_with: libgda,
	install: true,
	install_dir: join_paths(get_option('libdir'), project_package, 'providers')
	)
