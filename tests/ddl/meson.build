inc_testsh = include_directories ('.')
inc_testsh_dep = declare_dependency (include_directories : inc_testsh)

tests_sources = files([
	'check-ddl-creator.c'
	])

tbc = executable('check-ddl-creator',
	tests_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DCHECK_FILES="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ddlcreator', tbc,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

