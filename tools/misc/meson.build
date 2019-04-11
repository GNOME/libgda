gda_tools_cargs = [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DROOT_DIR="'+gda_top_src+'"'
		]
gda_tools_cargs += c_args

executable('gda-list-config-'+project_api_version,
	'gda-list-config.c',
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep
		],
	c_args: gda_tools_cargs,
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
	c_args: gda_tools_cargs,
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
	c_args: gda_tools_cargs,
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
	c_args: gda_tools_cargs,
	)
