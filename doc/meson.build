subdir('C')

if valadoc.found()
	vala_doc = custom_target('vala_doc',
		command: [ valadoc,
			'--pkg=libxml-2.0',
			'--pkg=gio-2.0',
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
		output: 'valadoc',
		build_by_default: true,
		install: true,
		install_dir: join_paths (get_option ('datadir'), 'devhelp','books'),
		)
endif