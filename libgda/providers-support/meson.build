psupport_headers = files([
	'gda-pstmt.h',
])
psupport_headers_priv = files([
	'gda-data-select-priv.h',
	'gda-meta-column-types.h'
])

install_headers(psupport_headers + psupport_headers_priv,
	install_dir: join_paths(get_option('includedir'),project_package,'libgda','providers-support')
	)


psupport_sources = files([
	'gda-pstmt.c'
])

psupport_gi_sources = []
psupport_gi_sources += psupport_headers
psupport_gi_sources += psupport_sources

libgda_psupport_sources = []
libgda_psupport_sources += psupport_sources
libgda_psupport_sources += psupport_headers
libgda_psupport_sources += psupport_headers_priv
	