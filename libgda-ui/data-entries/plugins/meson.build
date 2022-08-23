inc_libgdauipluginsh = include_directories ('.')
inc_libgdauipluginsh_dep = declare_dependency (include_directories : inc_libgdauipluginsh)

plugindir = join_paths(get_option('libdir'),project_package,'plugins')

data_entries_plugins_cargs = [
	'-include',
	meson.build_root() + '/config.h',
	'-DPREFIX="'+get_option('prefix')+'"',
	'-DSYSCONFDIR="'+get_option('sysconfdir')+'"',
	'-DDATADIR="'+get_option('datadir')+'"',
	'-DLIBDIR="'+plugindir+'"'
	]

data_entries_plugins_cargs += c_args

gcrypt_headers= []
gcrypt_sources= []
if libgcrypt_config.found()
gcrypt_headers += files([
	'gdaui-entry-password.h',
	'gdaui-data-cell-renderer-password.h'
	])
gcrypt_sources += files([
	'gdaui-entry-password.c',
	'gdaui-data-cell-renderer-password.c'
	])
endif

plugins_headers = files([
	'common-pict.h',
	'gdaui-data-cell-renderer-pict.h',
	'gdaui-entry-filesel.h',
	'gdaui-entry-cidr.h',
	'gdaui-entry-format.h',
	'gdaui-entry-text.h',
	'gdaui-entry-pict.h',
	'gdaui-entry-rt.h'
	])
plugins_headers += gcrypt_headers

libgda_ui_plugins_sources = files([
	'gdaui-data-cell-renderer-util.c',
	'gdaui-data-cell-renderer-util.h',
	'common-pict.c',
	'custom-marshal.c',
	'custom-marshal.h',
	'libmain.c',
	'gdaui-data-cell-renderer-pict.c',
	'gdaui-entry-filesel.c',
	'gdaui-entry-cidr.c',
	'gdaui-entry-format.c',
	'gdaui-entry-text.c',
	'gdaui-entry-pict.c',
	'gdaui-entry-rt.c'
	])
libgda_ui_plugins_sources += gcrypt_sources
libgda_ui_plugins_sources += plugins_headers
