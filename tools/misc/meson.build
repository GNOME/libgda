
executable('gda-list-config-'+project_api_version,
	'gda-list-config.c',
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	install: true
	)

gda_list_server_op_sources = files([
	'gda-list-server-op.c',
	'gda-tree-mgr-xml.c',
	'gda-tree-mgr-xml.h'
	])

executable('gda-list-server-op-'+project_api_version,
	gda_list_server_op_sources,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	install: true
	)


executable('information-schema-doc',
	'information-schema-doc.c',
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	)


executable('information-schema-types',
	'information-schema-types.c',
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep
		],
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	)