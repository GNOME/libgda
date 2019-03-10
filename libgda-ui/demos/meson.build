perl = find_program('perl')

if perl.found ()

democonf = configuration_data()
democonf.set('PERL', perl.path ())

geninclude = configure_file(
	configuration: democonf,
	input: 'geninclude.pl.in',
	output: 'genincluide.pl')

demos = files([
	'basic_form.c',
	'data_model_dir.c',
	'form.c',
	'form_rw.c',
	'form_pict.c',
	'form_data_layout.c',
	'form_model_change.c',
	'grid.c',
	'grid_rw.c',
	'grid_pict.c',
	'grid_data_layout.c',
	'grid_model_change.c',
	'linked_grid_form.c',
	'linked_model_param.c',
	'ddl_queries.c',
	'login.c',
	'provider_sel.c',
	'tree.c',
	'cloud.c',
	'combo.c',
	])

demosh = custom_target('demos.h',
	command: [
		perl, '@INPUT@',
	],
	input: [geninclude, demos],
	output: 'demos.h',
	capture: true,
	)

demosh_dep = declare_dependency (sources: demosh)

demo_deps = [
	libgda_dep,
	gtk_dep,
	gtksourceview_dep,
	inc_rooth_dep,
	inc_libgdah_dep,
	inc_sqliteh_dep,
	demosh_dep,
	]

install_data('demo_db.db',
	install_dir: join_paths(get_option('datadir'), project_package, 'demo')
	)

install_data('custom_layout.xml',
	install_dir: join_paths(get_option('datadir'), project_package, 'demo')
	)

install_data(demos,
	install_dir: join_paths(get_option('datadir'), project_package, 'demo')
	)

demo_sources = files([
	'main.c',
	'demo-common.h',
	])
demo_sources += demos

gdaui_demo = executable('org.gnome.gda.Demoui',
	demo_sources,
	link_with: [
		libgda,
		libgdaui
		],
	dependencies: demo_deps,
	c_args: [
		libgda_ui_cargs,
		'-DPREFIX=\"'+get_option('prefix')+'\"',
		'-DSYSCONFDIR=\"'+get_option('sysconfdir')+'\"',
		'-DDATADIR=\"'+get_option('datadir')+'\"',
		'-DLIBDIR=\"'+get_option('libdir')+'\"'
	],
	install: true
	)

endif