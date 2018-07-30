binreloc_cargs = [
	'-DLIBGDAPREFIX="'+get_option('prefix')+'"',
	'-DLIBGDADATA="'+join_paths(get_option('prefix'), get_option('datadir'))+'"',
	'-DLIBGDALIB="'+join_paths(get_option('prefix'), get_option('libdir'))+'"',
	'-DLIBGDALIBEXEC="'+join_paths(get_option('prefix'), get_option('libexecdir'))+'"',
	'-DLIBGDABIN="'+join_paths(get_option('prefix'), get_option('bindir'))+'"',
	'-DLIBGDASBIN="'+join_paths(get_option('prefix'), get_option('sbindir'))+'"',
	'-DLIBGDASYSCONF="'+join_paths(get_option('prefix'), get_option('sysconfdir'))+'"'
]

binreloc_headers = files([
	'gda-binreloc.h'
	])

libgda_binreloc_sources = files([
	'gda-binreloc.c'
	])

libgda_binreloc_sources += binreloc_headers
