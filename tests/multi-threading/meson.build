
# check_parser check_cnc_lock
methread_common_sources = files([
	'common.c',
	'common.h'
	])


tchkmthp = executable('check_parser',
	['check_parser.c'] + methread_common_sources + common_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"',
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('MultiThreadParser', tchkmthp,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)
