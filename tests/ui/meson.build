test_ui_common = files([
	'common.c',
	'common.h'
	])

tckuis = executable('check_sets',
	['check_sets.c'] + test_ui_common,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
		],
	link_with: [libgda, libgdaui],
	dependencies: [
		libgda_dep,
		libgda_ui_deps,
		inc_rooth_dep,
		inc_testsh_dep
		],
	install: false
	)
test('UISets', tckuis,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root(),
		]
	)

tckuirgm = executable('check_raw_grid_model',
	['check_raw_grid_model.c'] + test_ui_common,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"',
		'-DTOP_BUILD_DIR="'+meson.build_root()+'"'
		],
	link_with: [libgda, libgdaui],
	dependencies: [
		libgda_dep,
		libgda_ui_deps,
		inc_rooth_dep,
		inc_testsh_dep
		],
	install: false
	)
test('UIRawGridModel', tckuirgm,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root(),
		]
	)