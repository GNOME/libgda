conf = configuration_data()
# FIXME: define LIBGDA_LDAP_VINC
# conf.set_quoted('LIBGDA_LDAP_VINC','')
libgda_virtualh = configure_file(
		input: 'libgda-virtual.h.in',
		output : 'libgda-virtual.h',
		configuration : conf)

virtual_headers = files([
	'gda-vconnection-data-model.h',
	'gda-vconnection-hub.h',
	'gda-vprovider-data-model.h',
	'gda-vprovider-hub.h',
	'gda-virtual-connection.h',
	'gda-virtual-provider.h',
])

libgda_virtual_sources = files(
	'gda-vconnection-data-model.c',
	# 'gda-vconnection-data-model-private.h',
	# 'gda-vconnection-hub.c',
	# 'gda-vprovider-data-model.c',
	# 'gda-vprovider-hub.c',
	# 'gda-virtual-connection.c',
	# 'gda-virtual-provider.c'
	)
libgda_virtual_sources += virtual_headers

libgda_virtual = library(
	'gda_virtual', [libgda_virtual_sources],
	dependencies : [glib_dep, sqlite, libxml_dep, inc_rooth_dep, inc_libgdah_dep, inc_sqliteh_dep],
	c_args : [
		'-include',
		meson.build_root() + '/config.h',
	],
	link_with: libgda,
	install : false)