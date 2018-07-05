inc_queryh = include_directories ('.')
inc_queryh_dep = declare_dependency (include_directories : inc_queryh)

query_exec_sources = files([
	'query-console-page.c',
	'query-console-page.h',
	'query-editor.c',
	'query-editor.h',
	'query-favorite-selector.c',
	'query-favorite-selector.h',
	'query-result.c',
	'query-result.h',
	'perspective-main.c',
	'perspective-main.h',
	'query-exec-perspective.h',
	'query-exec-perspective.c'
	])


gda_browser_query_exec_marshalls = gnome_module.genmarshal('query-marshal',prefix : '_qe_marshal',
					sources : 'query-marshal.list')

query_exec_sources += gda_browser_query_exec_marshalls

browser_deps += inc_queryh_dep