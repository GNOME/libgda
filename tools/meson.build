inc_browserh = include_directories ('.')
inc_browserh_dep = declare_dependency (include_directories : inc_browserh)

subdir('misc')
subdir('common')
subdir('base')
subdir('help')

if enable_ui

browser_deps = [
	libgda_dep,
	gtk_dep,
	gtksourceview_dep,
	inc_rooth_dep,
	inc_libgdah_dep,
	inc_sqliteh_dep,
	inc_browserh_dep,
	inc_commonh_dep,
	inc_browser_baseh_dep
	]

if goocanvas_dep.found()
	browser_deps += goocanvas_dep
endif

browser_resourcesc = custom_target('browser_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'tools','resources'),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'tools.gresource.xml',
	output: 'browser_resource.c',
	depends: xml_res,
	install: false
	)

browser_resourcesh = custom_target('browser_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'tools','resources'),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'tools.gresource.xml',
	output: 'browser_resource.h',
	depends: xml_res,
	install: false
	)
browser_resources = []
browser_resources += browser_resourcesc
browser_resources += browser_resourcesh

gda_browser_sources = files([
	'tools-main.c',
	])
gda_browser_sources += browser_resources
gda_browser_sources += tools_nonuisources
gda_browser_sources += basetool_sources

gda_browser_link_args = [
	'-DIS_BROWSER'
	]

subdir('browser')

if windows
	gda_browser_link_args += ['-mwindows']
	win_res = win.compile_resources('gda-browser-res.rc',
		depend_files: 'gda-browser.ico',
		include_directories: join_paths(meson.source_root(),'tools')
	)
	gda_browser_sources += win_res
endif

executable('gda-browser-'+project_api_version,
	gda_browser_sources,
	link_with: [
		libgda,
		libgdaui
		],
	dependencies: browser_deps,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	link_args: gda_browser_link_args
	)


endif