inc_commonh = include_directories ('.')
inc_commonh_dep = declare_dependency (include_directories : inc_commonh)

tools_nonuisources = files([
	't-decl.h',
	't-app.h',
	't-app.c',
	't-context.h',
	't-context.c',
	't-term-context.h',
	't-term-context.c',
	't-web-context.h',
	't-web-context.c',
	't-errors.h',
	't-errors.c',
	't-favorites.h',
	't-favorites.c',
	't-utils.h',
	't-utils.c',
	't-config-info.h',
	't-config-info.c',
	't-connection.h',
	't-connection.c',
	't-virtual-connection.h',
	't-virtual-connection.c',
	't-favorites-actions.h',
	't-favorites-actions.c'
	])

gda_tools_marshalls = gnome_module.genmarshal('common-marshal',prefix : '_marshal',
					sources : 'marshal.list')

tools_nonuisources += gda_tools_marshalls

tools_nonuisources_ui = files([
	'web-server.h', 'web-server.c', 'html-doc.h', 'html-doc.c'
	])

if soup_dep.found() and get_option('libsoup')
tools_nonuisources += tools_nonuisources_ui
endif

