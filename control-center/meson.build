gda_control_center_sources= files([
	'gdaui-login-dialog.c',
	'gdaui-login-dialog.h',
	'gdaui-bar.c',
	'gdaui-bar.h',
	'dsn-config.c',
	'dsn-config.h',
	'gdaui-dsn-assistant.c',
	'gdaui-dsn-assistant.h',
	'gdaui-dsn-editor.c',
	'gdaui-dsn-editor.h',
	'main.c',
	'provider-config.c',
	'provider-config.h',
	'support.h',
	'support.c',
	])

control_center_resourcesc = custom_target('control_center_resourcesc',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'control-center'),
		'--sourcedir='+join_paths(meson.source_root(),'control-center','data'),
		'--sourcedir='+join_paths(meson.source_root(),'libgda-ui','data'),
		'--generate-source',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'control-center.gresource.xml',
	output: 'control_center_resource.c',
	depends: xml_res,
	install: false
	)

control_center_resourcesh = custom_target('control_center_resourcesh',
	command: [compile_resources,
		'--sourcedir='+meson.current_build_dir(),
		'--sourcedir='+join_paths(meson.source_root(),'control-center'),
		'--sourcedir='+join_paths(meson.source_root(),'control-center','data'),
		'--sourcedir='+join_paths(meson.source_root(),'libgda-ui','data'),
		'--generate-header',
		'--target', '@OUTPUT@',
		'--internal',
		'@INPUT@'
		],
	input: 'control-center.gresource.xml',
	output: 'control_center_resource.h',
	depends: xml_res,
	install: false
	)

gda_control_center_sources += [
	control_center_resourcesc,
	control_center_resourcesh,
	libgda_ui_internal_sources
	]
gda_control_center_cflags = [
	'-include',
	meson.build_root() + '/config.h',
	'-DPREFIX="'+get_option('prefix')+'"',
        '-DSYSCONFDIR="'+get_option('sysconfdir')+'"',
        '-DDATADIR="'+get_option('datadir')+'"',
        '-DLIBDIR="'+get_option('libdir')+'"',
	'-DGDA_ABI_VERSION="'+project_api_version+'"'
	]

if windows
	gda_control_center_cflags += ['-mwindows']
	gda_control_center_res = windows_module.compile_resources(
		depend_files: 'control-center-res.rc'
		)
	gda_control_center_sources += [ gda_control_center_res ]
endif

executable('gda-control-center-'+project_api_version,
	gda_control_center_sources,
	c_args: gda_control_center_cflags,
	dependencies : [
		libgda_dep,
		libgda_ui_deps
		],
	link_with: [
		libgda,
		libgdaui
		],
	install: true
	)