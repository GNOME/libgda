
tchkpr = executable('check_parsercheck_parser',
	['check_parser.c'],
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
test('Parser', tchkpr,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkval = executable('check_validation',
	['check_validation.c'],
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
test('Validation', tchkval,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkval = executable('check_normalization',
	['check_normalization.c'],
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
test('Normalization', tchkval,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkdlmc = executable('check_dml_comp',
	['check_dml_comp.c'],
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
test('DlmComp', tchkdlmc,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkscript = executable('check_script',
	['check_script.c'],
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
test('Script', tchkscript,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkrwd = executable('check_rewrite_for_default',
	['check_rewrite_for_default.c'],
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
test('RewriteDefault', tchkrwd,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

