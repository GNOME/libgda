libgda_ui_deps = [
	libgda_dep,
	gtk_dep,
	gio_dep
	]

subdir('marshallers')
subdir('data-entries')
subdir('internal')
if glade_dep.found()
	subdir('glade')
endif

libgda_ui_cargs = [
	'-include',
	meson.build_root() + '/config.h',
	'-DPLUGINSDIR="'+join_paths(meson.build_root(),'libgda-ui','plugins')+'"',
	'-DGDA_ABI_VERSION="'+project_api_version+'"'
	]
libgda_ui_cargs += iso_codes_cargs
libgda_ui_cargs += c_args
libgda_ui_headers = files([
	'gdaui-basic-form.h',
	'gdaui-combo.h',
	'gdaui-data-entry.h',
	'gdaui-data-store.h',
	'gdaui-data-filter.h',
	'gdaui-data-selector.h',
	'gdaui-data-proxy.h',
	'gdaui-data-proxy-info.h',
	'gdaui-decl.h',
	'gdaui-easy.h',
	'gdaui-enums.h',
	'gdaui-form.h',
	'gdaui-grid.h',
	'gdaui-cloud.h',
	'gdaui-login.h',
	'gdaui-provider-selector.h',
	'gdaui-raw-form.h',
	'gdaui-raw-grid.h',
	'gdaui-rt-editor.h',
	'gdaui-server-operation.h',
	'gdaui-tree-store.h',
	'gdaui-plugin.h',
        'libgda-ui.h'
	])

libgda_ui_sources = files([
	'gdaui-basic-form.c',
	'gdaui-combo.c',
	'gdaui-data-entry.c',
	'gdaui-data-store.c',
	'gdaui-data-filter.c',
	'gdaui-data-selector.c',
	'gdaui-data-proxy.c',
	'gdaui-data-proxy-info.c',
	'gdaui-form.c',
	'gdaui-grid.c',
	'gdaui-cloud.c',
	'gdaui-login.c',
	'gdaui-plugin.c',
	'gdaui-provider-selector.c',
	'gdaui-raw-form.c',
	'gdaui-raw-grid.c',
	'gdaui-rt-editor.c',
	'gdaui-server-operation.c',
	'gdaui-set.h',
	'gdaui-set.c',
	'gdaui-tree-store.c',
	'gdaui-init.c'
	])

libgda_ui_sources_all = files([
	'bullet.h',
	'bulleth.h',
	])
libgda_ui_sources_all += [
	libgda_ui_sources,
	libgda_ui_headers,
	marshallers_sources,
	libgda_ui_data_entries_sources_all,
	libgda_ui_internal_sources
	]
libgda_ui_gi_sources = [
	libgda_ui_sources,
	libgda_ui_headers,
	libgda_ui_data_entries_gi_sources,
	]

libgda_ui_resourcesc = custom_target('libgdaui_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.build_root(),'libgda-ui'),
		'--sourcedir='+join_paths(meson.source_root(),'libgda-ui'),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--c-name', '_gdaui',
		'--manual-register',
		'@INPUT@'
		],
	input: 'gdaui.gresource.xml',
	output: 'gdaui-resources.c',
	depends: int_xmlf,
	install: false
	)

libgda_ui_resourcesh = custom_target('libgdaui_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'libgda-ui'),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'gdaui.gresource.xml',
	output: 'gdaui-resources.h',
	depends: int_xmlf,
	install: false
	)
libgda_ui_resources = [
	libgda_ui_resourcesc,
	libgda_ui_resourcesh
	]

libgda_ui_sources_all += libgda_ui_resources

gdaui_enums = gnome_module.mkenums_simple('gdaui-enum-types',
	sources: libgda_ui_headers
	)
libgda_ui_sources_all += gdaui_enums

install_headers(libgda_ui_headers,
        install_dir: join_paths(get_option('includedir'), project_package,'libgda-ui') )

libgdaui = library('gda-ui-'+project_api_version,
	libgda_ui_sources_all,
	soversion : LT_VERSION+'.'+LT_AGE+'.'+LT_REVISION,
	version : LT_VERSION,
	dependencies: [
		libgda_ui_deps,
		inc_rooth_dep
		],
	c_args: [
		libgda_ui_cargs
	],
	link_with: [ libgda ],
	link_args: [
		'-Wl,--export-dynamic'
		],
	install: true
	)

libgda_ui_plugins = library('gda-ui-plugins-'+project_package,
	libgda_ui_plugins_sources,
	dependencies: [
		libgda_ui_deps,
		inc_rooth_dep,
		inc_libgdah_dep,
		inc_libgdauidataentriesh_dep,
		inc_libgdauipluginsh_dep
		],
	c_args: data_entries_plugins_cargs,
	link_with: [ libgda, libgdaui ],
	link_args: [
		'-Wl,--export-dynamic'
		],
	install: true,
	install_dir: plugindir
	)

libgdaui_gir = gnome_module.generate_gir(libgdaui,
	sources: libgda_ui_gi_sources,
	namespace: 'Gdaui',
	identifier_prefix: 'Gdaui',
	nsversion: project_api_version,
	header: 'libgda-ui/libgda-ui.h',
	includes: [
		'GLib-2.0',
		'GObject-2.0',
		'Gio-2.0',
		'libxml2-2.0',
		'Gtk-3.0',
		libgda_gir[0],
		],
	dependencies: libgda_dep,
	install: true,
	install_dir_gir: join_paths(get_option('datadir'), 'gir-1.0'),
	install_dir_typelib: join_paths(get_option('libdir'), 'girepository-1.0')
	)

if enable_vala
libgdaui_vapi = custom_target('gdaui'+project_api_version+'_vapi',
	command: [vapigen,
		'--girdir='+join_paths(meson.build_root(),'libgda'),
		'--metadatadir='+meson.current_source_dir(),
		'--library=libgdaui-'+project_api_version,
		'--directory='+meson.current_build_dir(),
		'--vapidir='+join_paths(meson.build_root(),'libgda'),
		'--pkg=libxml-2.0',
		'--pkg=gio-2.0',
		'--pkg=libgda-6.0',
		'--pkg=gtk+-3.0',
		'@INPUT0@', '@INPUT1@'
		],
	input: [
		libgdaui_gir[0],
		'Gda-'+project_api_version+'-custom.vala'
		],
	output: 'libgdaui-'+project_api_version+'.vapi',
	install: true,
	depends: libgdaui,
	install_dir: join_paths(get_option('datadir'),'vala','vapi')
	)
endif

subdir('demos')
