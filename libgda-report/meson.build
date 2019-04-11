inc_libgda_reporth = include_directories ('.')
inc_libgda_reporth_dep = declare_dependency (include_directories : inc_libgda_reporth)

subdir('engine')
subdir('DocBook')
subdir('RML')

report_headers = files([
	'gda-report-document.h'
	])
install_headers(report_headers+['libgda-report.h'],
	install_dir: join_paths(get_option('includedir'), project_package,'libgda-report')
	)

libgda_report_sources = files([
	'gda-report-document-private.h',
	'gda-report-document.c'
	])
libgda_report_sources += report_headers
libgda_report_sources += libgda_report_engine_sources
libgda_report_sources += libgda_report_docbook_sources
libgda_report_sources += libgda_report_rml_sources

libgda_report_cargs = [
	'-include',
	meson.build_root() + '/config.h',
	]
libgda_report_cargs += c_args

libgda_report = library(
	'gda-report-'+project_api_version,
	libgda_report_sources,
	soversion : LT_VERSION+'.'+LT_AGE+'.'+LT_REVISION,
	version : LT_VERSION,
	dependencies : [libgda_dep,
			gdkpixbuf_dep,
			inc_libgda_reporth_dep,
			inc_rooth_dep,
			inc_libgdah_dep,
			inc_sqliteh_dep
			],
	c_args: libgda_report_cargs,
	link_with: libgda,
	install : true
	)


test_rt_parser = executable('test-rt-parser',
	test_rt_parser_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_libgdah_dep
		],
	link_with: [libgda, libgda_report],
	install: false
	)
test('RtParser', test_rt_parser)
