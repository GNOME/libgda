# UI tests: gdaui-test-data-entries gdaui-test-widget-entry gdaui-test-errors gdaui-test-rt-editor
# virtual-test-2

extbl = executable('gda-test-blob',
	['gda-test-blob.c'] + tests_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep,
		inc_testsh_dep,
		],
	install: false
	)

extv = executable('virtual-test',
	['virtual-test.c'] + tests_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep,
		inc_testsh_dep,
		],
	install: false
	)

extv2 = executable('virtual-test-2',
	['virtual-test-2.c'] + tests_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	link_with: libgda,
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_sqliteh_dep,
		inc_testsh_dep,
		],
	install: false
	)
