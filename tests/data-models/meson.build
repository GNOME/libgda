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

# check_virtual check_data_proxy check_model_copy check_pmodel check_empty_rs check_model_errors check_vcnc check_pivot


tchki = executable('check_model_import',
	['check_model_import.c'] + common_sources,
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
test('ModelImport', tchki,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tchkv = executable('check_virtual',
	['check_virtual.c'] + common_sources,
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
test('Virtual', tchkv,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)