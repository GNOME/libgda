inc_libgdauidataentriesh = include_directories ('.')
inc_libgdauidataentriesh_dep = declare_dependency (include_directories : inc_libgdauidataentriesh)

data_entries_cargs = [
	'-DPREFIX="'+get_option('prefix')+'"',
        '-DSYSCONFDIR="'+get_option('sysconfdir')+'"',
        '-DDATADIR="'+get_option('datadir')+'"',
        '-DLIBDIR="'+get_option('libdir')+'"'
	]

extra_headers = files([
	'gdaui-data-cell-renderer-util.h',
	'gdaui-entry-shell.h',
	'gdaui-entry-wrapper.h'
	])

install_headers(extra_headers,
	install_dir: join_paths(get_option('includedir'),project_package,'libgda-ui')
	)

libgda_ui_data_entries_headers = files([
	'gdaui-data-cell-renderer-bin.h',
	'gdaui-data-cell-renderer-boolean.h',
	'gdaui-data-cell-renderer-combo.h',
	'gdaui-data-cell-renderer-info.h',
	'gdaui-data-cell-renderer-textual.h',
	'gdaui-entry-bin.h',
	'gdaui-entry-boolean.h',
	'gdaui-entry-combo.h',
	'gdaui-entry-none.h',
	'gdaui-entry-string.h',
	'gdaui-entry-number.h',
	'gdaui-entry-common-time.h',
	'gdaui-entry-time.h',
	'gdaui-entry-timestamp.h',
	'gdaui-entry-date.h',
	'gdaui-entry.h',
	'gdaui-formatted-entry.h',
	'gdaui-numeric-entry.h'
	])

libgda_ui_data_entries_sources = files([
	'common-bin.c',
	'common-bin.h',
	'gdaui-data-cell-renderer-util.c',
	'gdaui-data-cell-renderer-bin.c',
	'gdaui-data-cell-renderer-boolean.c',
	'gdaui-data-cell-renderer-combo.c',
	'gdaui-data-cell-renderer-info.c',
	'gdaui-data-cell-renderer-textual.c',
	'gdaui-entry-bin.c',
	'gdaui-entry-boolean.c',
	'gdaui-entry-combo.c',
	'gdaui-entry-none.c',
	'widget-embedder.c',
	'widget-embedder.h',
	'gdaui-entry-shell.c',
	'gdaui-entry-string.c',
	'gdaui-entry-number.c',
	'gdaui-entry-common-time.c',
	'gdaui-entry-time.c',
	'gdaui-entry-timestamp.c',
	'gdaui-entry-date.c',
	'gdaui-entry-wrapper.c',
	'gdaui-entry.c',
	'gdaui-formatted-entry.c',
	'gdaui-numeric-entry.c'
	])
libgda_ui_data_entries_sources += libgda_ui_data_entries_headers
libgda_ui_data_entries_sources += extra_headers

data_entries_xmlin_names = [
	'gdaui-entry-string',
	'gdaui-entry-integer',
	'gdaui-entry-number'
	]
data_entries_xmlin_namesext = []
foreach fn : data_entries_xmlin_names
	data_entries_xmlin_namesext += [fn+'.xml.in']
endforeach

data_entries_xml_inf = files(data_entries_xmlin_namesext)

foreach xf : data_entries_xmlin_names
	install_data(xf+'.xml.in',
		rename: [
			xf+'.xml'
			],
		install_dir: join_paths(get_option('datadir'), project_package,'ui')
		)
endforeach



foreach xt : data_entries_xmlin_names
	i18n.merge_file(
		input: xt+'.xml.in',
		output: xt+'.xml.in.h',
		type: 'xml',
		po_dir: join_paths(meson.build_root(),'po')
		)
endforeach

subdir('plugins')