subdir('C')

pageconf = configuration_data()
pageconf.set('GDA_API_VERSION', project_api_version)
configure_file(
	input: 'index.html.in',
	output: 'index.html',
	configuration: pageconf
	)

if enable_vala and valadoc.found()
	vala_doc = custom_target('vala_doc',
		command: [ valadoc,
			'--pkg=libxml-2.0',
			'--pkg=gio-2.0',
			'--force',
			'--girdir='+join_paths(meson.project_build_root(),'libgda'),
			'--metadatadir='+join_paths(meson.project_source_root(),'libgda'),
			'--vapidir='+join_paths(meson.project_build_root(),'libgda'),
			'--package-version='+project_version,
			'--package-name='+'Gda-'+project_api_version,
			'--doclet=devhelp',
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: libgda_vapi,
		output: 'Gda-'+project_api_version,
		build_by_default: true,
		install: true,
		install_dir: join_paths (get_option ('datadir'), 'devhelp','books'),
		)
	if enable_ui
	valaui_doc = custom_target('valaui_doc',
		command: [ valadoc,
			'--pkg=libxml-2.0',
			'--pkg=gio-2.0',
			'--pkg=gtk+-3.0',
			'--pkg=libgda-6.0',
			'--force',
			'--girdir='+join_paths(meson.project_build_root(),'libgda-ui'),
			'--metadatadir='+join_paths(meson.project_source_root(),'libgda-ui'),
			'--vapidir='+join_paths(meson.project_build_root(),'libgda'),
			'--vapidir='+join_paths(meson.project_build_root(),'libgda-ui'),
			'--package-version='+project_version,
			'--package-name='+'Gdaui-'+project_api_version,
			'--doclet=devhelp',
			'-o', meson.current_build_dir(),
			'@INPUT@'
			],
		input: libgdaui_vapi,
		output: 'Gdaui-'+project_api_version,
		build_by_default: true,
		install: true,
		install_dir: join_paths (get_option ('datadir'), 'devhelp','books'),
		)
	endif
endif