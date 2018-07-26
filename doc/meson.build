subdir('C')

pageconf = configuration_data()
pageconf.set('GDA_API_VERSION', project_api_version)
configure_file(
	input: 'index.html.in',
	output: 'index.html',
	configuration: pageconf
	)

if valadoc.found()
	vala_doc = custom_target('vala_doc',
		command: [ valadoc,
			'--pkg=libxml-2.0',
			'--pkg=gio-2.0',
			'--force',
			'--girdir='+join_paths(meson.build_root(),'libgda'),
			'--metadatadir='+join_paths(meson.source_root(),'libgda'),
			'--vapidir='+join_paths(meson.build_root(),'libgda'),
			'--package-version='+project_version,
			'--package-name='+'libgda-'+project_api_version,
			'--doclet=devhelp',
			'-o', '@OUTPUT@',
			'@INPUT@'
			],
		input: libgda_vapi,
		output: 'vala',
		build_by_default: true,
		install: true,
		install_dir: join_paths (get_option ('datadir'), 'devhelp','books'),
		)
endif