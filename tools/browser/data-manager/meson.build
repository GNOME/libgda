inc_data_managerh = include_directories ('.')
inc_data_managerh_dep = declare_dependency (include_directories : inc_data_managerh)

data_manager_sources = files([
	'analyser.c',
	'analyser.h',
	'data-console.c',
	'data-console.h',
	'data-favorite-selector.c',
	'data-favorite-selector.h',
	'data-manager-perspective.c',
	'data-manager-perspective.h',
	'data-source.c',
	'data-source-editor.c',
	'data-source-editor.h',
	'data-source.h',
	'data-source-manager.c',
	'data-source-manager.h',
	'data-widget.c',
	'data-widget.h',
	'perspective-main.c',
	'perspective-main.h',
	'ui-spec-editor.c',
	'ui-spec-editor.h',
	'xml-spec-editor.c',
	'xml-spec-editor.h'
	])


gda_browser_data_manager_marshalls = gnome_module.genmarshal('data-manager-marshal',prefix : '_dm_marshal',
					sources : 'data-manager-marshal.list')

data_manager_sources += gda_browser_data_manager_marshalls

browser_deps += inc_data_managerh_dep