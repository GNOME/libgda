
methread_common_sources = files([
	'common.c',
	'common.h'
	])


tchkmthp = executable('check_parser',
	['check_parser.c'] + methread_common_sources + common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DROOT_DIR="'+gda_top_src+'"',
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
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkmthlk = executable('check_cnc_lock',
	['check_cnc_lock.c'] + methread_common_sources + common_sources,
	c_args: [
		'-include',
		join_paths(gda_top_build, 'config.h'),
		'-DROOT_DIR="'+gda_top_src+'"',
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
test('MultiThreadConnectionLock', tchkmthlk,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
