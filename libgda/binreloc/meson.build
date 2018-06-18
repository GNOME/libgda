
binreloc_cargs = [
	'-DLIBGDAPREFIX="'+get_option('prefix')+'"',
	'-DLIBGDADATA="'+get_option('datadir')+'"',
	'-DLIBGDALIB="'+get_option('libdir')+'"',
	'-DLIBGDALIBEXEC="'+get_option('libexecdir')+'"',
	'-DLIBGDABIN="'+get_option('bindir')+'"',
	'-DLIBGDASBIN="'+get_option('sbindir')+'"',
	'-DLIBGDASYSCONF="'+get_option('sysconfdir')+'"'
]

binreloc_headers = files([
	'gda-binreloc.h'
	])

libgda_binreloc_sources = files([
	'gda-binreloc.c'
	])

libgda_binreloc_sources += binreloc_headers
