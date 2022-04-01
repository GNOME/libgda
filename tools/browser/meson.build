inc_browserdirh = include_directories ('.')
inc_browserdirh_dep = declare_dependency (include_directories : inc_browserdirh)


subdir('dummy-perspective')
subdir('schema-browser')
subdir('query-exec')
subdir('data-manager')

if enable_ldap
subdir('ldap-browser')
gda_browser_sources += ldap_perspective_sources
browser_deps += inc_ldapbrowserdir_dep
endif

if goocanvas_dep.found()
subdir('canvas')
gda_browser_sources += canvas_sources
endif

gda_browser_sources += dummy_perspective_sources
gda_browser_sources += schema_sources
gda_browser_sources += query_exec_sources
gda_browser_sources += data_manager_sources

gda_browser_link_args += [
	'-DPREFIX="'+get_option('prefix')+'"',
	'-DSYSCONFDIR="'+get_option('sysconfdir')+'"',
	'-DDATADIR="'+get_option('datadir')+'"',
	'-DLIBDIR="'+get_option('libdir')+'"'
	]


gda_browser_marshalls = gnome_module.genmarshal('browser-marshal',prefix : '_common_marshal',
					sources : 'browser-marshal.list')


gda_browser_sources += gda_browser_marshalls

gda_browser_sourcesf = files([
	'decl.h',
	'perspectives-names.h',
	'dnd.h',
	'dnd.c',
	'ui-support.h',
	'ui-support.c',
	'auth-dialog.h',
	'auth-dialog.c',
	'login-dialog.h',
	'login-dialog.c',
	'browser-perspective.h',
	'browser-perspective.c',
	'browser-page.h',
	'browser-page.c',
	'browser-window.h',
	'browser-window.c',
	'browser-connections-list.h',
	'browser-connections-list.c',
	'connection-binding-properties.h',
	'connection-binding-properties.c',
	'gdaui-data-import.h',
	'gdaui-data-import.c',
	'gdaui-entry-import.h',
	'gdaui-entry-import.c',
	'objects-cloud.h',
	'objects-cloud.c',
	'fk-declare.h',
	'fk-declare.c',
	'gdaui-bar.h',
	'gdaui-bar.c',
	'mgr-favorites.h',
	'mgr-favorites.c',
	'widget-overlay.h',
	'widget-overlay.c',
	'ui-formgrid.h',
	'ui-formgrid.c',
	'text-search.h',
	'text-search.c',
	'ui-customize.h',
	'ui-customize.c'
	])

gda_browser_sources += gda_browser_sourcesf

subdir('data')

i18n.merge_file(
	po_dir: join_paths(meson.source_root(),'po'),
	type: 'desktop',
	input: 'org.gnome.gda.Browser.desktop.in',
	output: 'org.gnome.gda.Browser.desktop',
	install: true,
	install_dir: join_paths(get_option('datadir'),'applications')
	)

custom_target('org.gnome.gda.Browser.appdata',
		command: [intlmerge,
			'-u', '-x',
			join_paths(meson.source_root(),'po'),
			'@INPUT@', '@OUTPUT@'
			],
		input: 'org.gnome.gda.Browser.appdata.xml.in',
		output: 'org.gnome.gda.Browser.appdata.xml',
		install: true,
		install_dir: join_paths(get_option('datadir'),'metainfo')
		)

browser_deps += inc_browserdirh_dep
