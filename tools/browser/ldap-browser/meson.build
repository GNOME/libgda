inc_ldapbrowserdirh = include_directories ('.')
inc_ldapbrowserdir_dep = declare_dependency (include_directories : inc_ldapbrowserdirh)

ldap_perspective_sources = files([
	'perspective-main.h',
	'perspective-main.c',
	'ldap-browser-perspective.h',
	'ldap-browser-perspective.c',
	'ldap-favorite-selector.h',
	'ldap-favorite-selector.c',
	'ldap-entries-page.c',
	'ldap-entries-page.h',
	'mgr-ldap-entries.h',
	'mgr-ldap-entries.c',
	'hierarchy-view.h',
	'hierarchy-view.c',
	'entry-properties.h',
	'entry-properties.c',
	'mgr-ldap-classes.h',
	'mgr-ldap-classes.c',
	'classes-view.h',
	'classes-view.c',
	'ldap-classes-page.h',
	'ldap-classes-page.c',
	'class-properties.h',
	'class-properties.c',
	'filter-editor.h',
	'filter-editor.c',
	'ldap-search-page.h',
	'ldap-search-page.c',
	'vtable-dialog.h',
	'vtable-dialog.c'
	])


ldap_perspective_marshalls = gnome_module.genmarshal('ldap-marshal',prefix : '_ldap_marshal',
					sources : 'marshal.list')

ldap_perspective_sources += [
	ldap_perspective_marshalls
	]