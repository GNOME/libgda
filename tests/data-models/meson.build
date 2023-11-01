testsf = files([
	'check_virtual.csv',
	'city.csv',
	'country.csv',
	'data1.xml',
	'pmodel_dbstruct.xml',
	'pmodel_data_customers.xml',
	'pmodel_data_locations.xml',
	'cities1.xml',
	'cities2.xml',
	'cities3.xml',
	'countriesA.xml',
	'countriesB.xml',
	'pivot.db'
	])

tchki = executable('check_model_import',
	['check_model_import.c'] + common_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ModelImport', tchki,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkv = executable('check_virtual',
	['check_virtual.c'] + common_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('Virtual', tchkv,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkdp = executable('check_data_proxy',
	['check_data_proxy.c'] + common_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
# This test is no thread save so disabling
#test('DataProxy', tchkdp,
#	timeout: 100,
#	workdir: meson.current_build_dir(),
#	env: [
#		'GDA_TOP_SRC_DIR='+gda_top_src,
#		'GDA_TOP_BUILD_DIR='+gda_top_build
#		]
#	)

tchkmc = executable('check_model_copy',
	['check_model_copy.c'] + common_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ModelCopy', tchkmc,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkpm = executable('check_pmodel',
	['check_pmodel.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('PModel', tchkpm,
	timeout: 200,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		],
	is_parallel: false
	)

tchkemrs = executable('check_empty_rs',
	['check_empty_rs.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('EmptyRS', tchkemrs,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkmerr = executable('check_model_errors',
	['check_model_errors.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('ModelErrors', tchkmerr,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
tchkvcnc = executable('check_vcnc',
	['check_vcnc.c'] + tests_sources,
	c_args: test_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('VirtualConnection', tchkvcnc,
	timeout: 300,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkpivot_cargs = [
	'-DROOT_DIR="'+gda_top_src+'"'
	]
tchkpivot_cargs += test_cargs

tchkpivot = executable('check_pivot',
	['check_pivot.c'] + tests_sources,
	c_args: tchkpivot_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)
test('Pivot', tchkpivot,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)

tchkdsi_cargs = [
	'-DBUILD_DIR="'+meson.current_build_dir()+'"'
	]
tchkdsi_cargs += test_cargs

tchkdsi = executable('check_data_select_iter',
	['check_data_select_iter.c'],
	c_args: tchkdsi_cargs,
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep,
		inc_testsh_dep
		],
	install: false
	)

test('DataSelectIter', tchkdsi,
	env: [
		'GDA_TOP_SRC_DIR='+gda_top_src,
		'GDA_TOP_BUILD_DIR='+gda_top_build
		]
	)
