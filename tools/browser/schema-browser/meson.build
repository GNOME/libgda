inc_schemah = include_directories ('.')
inc_schemah_dep = declare_dependency (include_directories : inc_schemah)

schema_sources = files([
	'perspective-main.h',
	'perspective-main.c',
	'schema-browser-perspective.h',
	'schema-browser-perspective.c',
	'favorite-selector.c',
	'favorite-selector.h',
	'objects-index.c',
	'objects-index.h',
	'table-info.c',
	'table-info.h',
	'table-columns.c',
	'table-columns.h',
	'table-preferences.c',
	'table-preferences.h',
	'mgr-columns.c',
	'mgr-columns.h'
	])

gda_browser_schema_marshalls = gnome_module.genmarshal('marshal',prefix : '_sb_marshal',
					sources : 'marshal.list')

schema_sources += gda_browser_schema_marshalls

if goocanvas_dep.found()
schema_sources_goocanvas = files([
	'table-relations.c',
	'table-relations.h',
	'relations-diagram.c',
	'relations-diagram.h'
	])
schema_sources += schema_sources_goocanvas
endif

browser_deps += inc_schemah_dep