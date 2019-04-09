
tchkpr = executable('check_parser',
	['check_parser.c'],
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
test('Parser', tchkpr,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkval = executable('check_validation',
	['check_validation.c'],
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
test('Validation', tchkval,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkval = executable('check_normalization',
	['check_normalization.c'],
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
test('Normalization', tchkval,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkdlmc = executable('check_dml_comp',
	['check_dml_comp.c'],
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
test('DlmComp', tchkdlmc,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkscript = executable('check_script',
	['check_script.c'],
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
test('Script', tchkscript,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkrwd = executable('check_rewrite_for_default',
	['check_rewrite_for_default.c'],
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
test('RewriteDefault', tchkrwd,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

