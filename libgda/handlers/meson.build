libgda_handlers_headers = files([
	'gda-handler-bin.h',
	'gda-handler-boolean.h',
	'gda-handler-numerical.h',
	'gda-handler-string.h',
	'gda-handler-text.h',
	'gda-handler-time.h',
	'gda-handler-type.h'
	])

install_headers(libgda_handlers_headers,
	install_dir: join_paths(get_option('includedir'),project_package,'libgda','handlers')
	)

libgda_handlers_sources = files([
	'gda-handler-bin.c',
	'gda-handler-boolean.c',
	'gda-handler-numerical.c',
	'gda-handler-string.c',
	'gda-handler-text.c',
	'gda-handler-time.c',
	'gda-handler-type.c'
	])

libgda_handlers_sources += libgda_handlers_headers