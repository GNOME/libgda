project('libgda', 'c',
        license: 'LGPL2+',
        version : '5.91.0',
        meson_version: '>= 0.44.1',
        default_options: [
                        'c_std=gnu11',
                        'warning_level=2',
                        ],
)

project_version = meson.project_version()
version_split = project_version.split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]
API_MAJOR_VERSION = '6'
API_MINOR_VERSION = '0'
project_api_version = API_MAJOR_VERSION+'.'+API_MINOR_VERSION
project_abi_version = MAJOR_VERSION+'.'+MINOR_VERSION
project_abi_name = '"' + meson.project_name() + '-' + project_abi_version + '"'
project_api_name = '"' + meson.project_name() + '-' + project_api_version + '"'
project_package = meson.project_name() + '-' + project_api_version

gda_current = '5'
gda_revision ='1'
gda_age = '1'

compiler = meson.get_compiler('c')

glib_genmarshal = find_program('glib-genmarshal', required : true)

glib_mkenums = find_program('glib-mkenums', required : true)

girscanner = find_program('g-ir-scanner', required:false)

valadoc = find_program('valadoc', required: false)

inc_rooth = include_directories ('.')
inc_rooth_dep = declare_dependency (include_directories : inc_rooth)

req_ver_glib = '>= 2.38.0'
glib_dep = dependency('glib-2.0', version : req_ver_glib)
gio_dep = dependency('gio-2.0', version : '>=2.16')
gobject_dep = dependency('gobject-2.0', version : req_ver_glib)
gthread_dep = dependency('gthread-2.0', version : req_ver_glib)
gmodule_no_export_dep = dependency('gmodule-no-export-2.0', version : req_ver_glib)
libxml_dep = dependency('libxml-2.0')

goocanvas_dep = dependency('goocanvas-2.0', required : false)
graphviz_dep = dependency('libgvc', required : false)
libxslt_dep = dependency('libxslt', required: false)
jsonglib_dep = dependency('json-glib-1.0',required : false)
postgres_dep = dependency ('libpq', required: false)
mysql_dep = dependency ('mysqlclient', required: false)

# Platform data
windows = build_machine.system().contains('windows')
if windows
win = import('windows')
endif

lbdb_found = false
lbdb_versions = [
	'6.0',
	'5.3',
	'5.1',
	'5.0',
	'4.9',
	'4.8',
	'4.7'
	]
lbdb_cargs = []
# Disabling BDB provider because it is broken
# foreach lbdbv : lbdb_versions
# 	bdb_libt = compiler.find_library('db-'+lbdbv, required: false)
# 	if bdb_libt.found()
# 		if compiler.has_header('db.h')
# 			bdb_lib = bdb_libt
# 			lbdb_found = true
# 			if windows
# 				lbdb_filename = 'db-'+lbdbv+'.dll'
# 			else
# 				lbdb_filename = 'db-'+lbdbv+'.so'
# 			endif
# 			lbdb_cargs += [
# 				'-I'+get_option('includedir'),
# 				]
# 		endif
# 	endif
# endforeach

sqlite_dep = dependency ('sqlite3', version: '>= 3.6.11', required: false)
sqlite_internal = true
if sqlite_dep.found()
	sqlite3_table_column_metadata = compiler.find_library('sqlite3_table_column_metadata', required: false)
	if sqlite3_table_column_metadata.found()
		sqlite_internal = false
	endif
endif

if sqlite_internal
	ldl = compiler.find_library('dl')
endif

sed = find_program('sed')
compile_resources = find_program('glib-compile-resources')

libgda_dep = [
	glib_dep,
	gobject_dep,
	gio_dep,
	gmodule_no_export_dep,
	libxml_dep,
	inc_rooth_dep
	]

# LDAP
ldap_found = false
ldap_deps = []
ldap_lib_dep = compiler.find_library('ldap', required: false)
if ldap_lib_dep.found()
	ldapber_dep = compiler.find_library('lber')
	if compiler.has_header('ldap.h')
		ldap_found = true
		LIBGDA_LDAP_INC='#include <libgda/gda-data-model-ldap.h>'
		LIBGDA_LDAP_INC2='#include <libgda/gda-tree-mgr-ldap.h>'
		LIBGDA_LDAP_VINC='#include <virtual/gda-ldap-connection.h>'
		LIBGDA_LDAP_TYPE='gda_data_model_ldap_get_type'
		LIBGDA_LDAP_TYPE2='gda_ldap_connection_get_type'
		LIBGDA_LDAP_TYPE3='gda_tree_mgr_ldap_get_type'
		ldap_deps += [
			ldap_lib_dep,
			ldapber_dep
			]
	endif
endif

tconf = configuration_data() # Empty configuration

conf = configuration_data()
if glib_genmarshal.found()
  conf.set('HAVE_GLIB_GENMARSHAL',1)
else
  error ('glib-genmarshal not found')
endif
if glib_mkenums.found()
  conf.set('HAVE_GLIB_MKENUMS',1)
else
  error ('glib-mkenums not found')
endif
conf.set('GDA_ABI_MAJOR_VERSION', MAJOR_VERSION)
conf.set('GDA_ABI_MINOR_VERSION', MINOR_VERSION)
conf.set('VERSION', project_version)
conf.set('prefix', get_option('prefix'))
conf.set('exec_prefix', get_option('libexecdir'))
conf.set('libdir', get_option('libdir'))
conf.set('includedir', get_option('includedir'))
conf.set('GDA_DEBUG_FLAGS', '')
if lbdb_found
	conf.set('LIBGDA_BDB_INC', '#include <libgda/gda-data-model-bdb.h>')
	conf.set('LIBGDA_BDB_TYPE', 'gda_data_model_bdb_get_type')
else
	conf.set('LIBGDA_BDB_INC', '')
	conf.set('LIBGDA_BDB_TYPE', '')
endif
if ldap_found
	conf.set('LIBGDA_LDAP_VINC',LIBGDA_LDAP_VINC)
	conf.set('LIBGDA_LDAP_INC',LIBGDA_LDAP_INC)
	conf.set('LIBGDA_LDAP_INC2',LIBGDA_LDAP_INC2)
	conf.set('LIBGDA_LDAP_TYPE',LIBGDA_LDAP_TYPE)
	conf.set('LIBGDA_LDAP_TYPE2',LIBGDA_LDAP_TYPE2)
	conf.set('LIBGDA_LDAP_TYPE3',LIBGDA_LDAP_TYPE3)
endif

confh = configuration_data()
confh.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
confh.set_quoted('GETTEXT_PACKAGE', project_package)
confh.set_quoted('PACKAGE_VERSION', project_version)
if compiler.has_header('locale.h')
	confh.set ('HAVE_LOCALE_H', '1')
endif
configh = configure_file(output : 'config.h',
			configuration : confh)

c_args = ['-Wall',
	'-Wextra',
	'-Wformat-security',
	'-Wno-address',
	'-Wno-unused-variable',
	'-Wno-unused-parameter',
	'-Wno-pointer-sign',
	'-Wno-missing-field-initializers',
	'-DGSEAL_ENABLE',
	'-DLIBGDA_ABI_NAME='+project_api_name]

glade_catalog = ''
glade_pixmap = ''

gnome_module = import('gnome')

if get_option('enable-json')
  jsonglib_dep = dependency('json-glib-1.0')
endif


iso_codes_dep = dependency('iso-codes')
icodes = iso_codes_dep.get_pkgconfig_variable('domains')
iso_codes_prefix = ''
if icodes.contains('4217')
iso_codes_prefix = iso_codes_dep.get_pkgconfig_variable('prefix')
endif
iso_codes_cargs = [
	'-DISO_CODES_PREFIX="'+iso_codes_prefix+'"'
	]

libcryp_config = find_program('libcryp-config', required : false)
if libcryp_config.found()
  conf.set('HAVE_LIBCRYPT_CONFIG', 1)
endif

soup_dep = dependency('libsoup-2.4', required: false)
if soup_dep.found() and get_option('with-libsoup')
  libgda_dep += [soup_dep]
endif

if get_option('enable-debug')
  c_args += '-DGDA_DEBUG'
endif

add_global_arguments(c_args, language : 'c')

# Files references by sqlite embedded and provider
sqlite_xml_fnames = [
	'sqlite_specs_dsn',
	'sqlite_specs_create_db',
	'sqlite_specs_drop_db',
	'sqlite_specs_create_table',
	'sqlite_specs_drop_table',
	'sqlite_specs_create_index',
	'sqlite_specs_drop_index',
	'sqlite_specs_rename_table',
	'sqlite_specs_add_column',
	'sqlite_specs_create_view',
	'sqlite_specs_drop_view'
	]
sqlite_xml_fnamesext = []
foreach fn : sqlite_xml_fnames
	sqlite_xml_fnamesext += [join_paths(meson.source_root(), 'providers','sqlite',fn+'.xml.in')]
endforeach

sqlite_xml_inf = files(sqlite_xml_fnamesext)


intlmerge = find_program('intltool-merge')
# Vala bindings
vapigen = find_program('vapigen', required: false)

found_jdbc = false


dates = compiler.run('''
#include<glib.h>
#include <glib/gprintf.h>

void main (void) {
  GDateTime *dt;
  dt = g_date_time_new_now_local ();
  g_print (g_date_time_format(dt, "%F"));
}
''',
dependencies: glib_dep
)

subdir('po')
subdir('libgda')
subdir('providers')

gtk_dep = dependency('gtk+-3.0', required: false)
enable_ui = get_option('with-ui') and gtk_dep.found()
if enable_ui
	gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')
	glade_dep = dependency('gladeui-2.0', required : false)
	goocanvas_dep = dependency('goocanvas-2.0', required: false)
	if goocanvas_dep.found()
		math_dep = compiler.find_library('m')
	endif
	gtksourceview_dep = dependency('gtksourceview-3.0')
	graphviz_dep = dependency('libgvc', required: false)
	graphviz_cargs = []
	if get_option('with-graphviz') and graphviz_dep.found()
		graph_new_api_code = '''#include <gvc.h>
int main() {
	Agraph_t *graph;
	graph = agopen ("BrowserCanvasLayout", Agdirected, NULL);
	return 0;
}
'''
		graph_new_api_code_results = compiler.compiles(graph_new_api_code)
		if graph_new_api_code_results
			graphviz_cargs += [
				'-DHAVE_GRAPHVIZ_NEW_API="1"'
				]
		endif
	endif

	if glade_dep.found()
		glade_catalog = glade_dep.get_pkgconfig_variable('catalogdir')
		glade_pixmap = glade_dep.get_pkgconfig_variable('pixmapdir')
	endif
	subdir('libgda-ui')
	subdir('libgda-report')
	windows_module = import('windows')
	subdir('control-center')
endif

subdir('doc')

if libxslt_dep.found()
	subdir('libgda-xslt')
endif

# PC files
libgda_pc = configure_file(
	input: 'libgda-'+project_api_version+'.pc.in',
	output: 'libgda-'+project_api_version+'.pc',
	configuration: conf,
	install: true,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)

if libxslt_dep.found()
	libgda_xslt_pc = configure_file(
		input: 'libgda-xslt-'+project_api_version+'.pc.in',
		output: 'libgda-xslt-'+project_api_version+'.pc',
		configuration: conf,
		install: true,
		install_dir: join_paths(get_option('libdir'),'pkgconfig')
		)
endif

if enable_ui
libgdaui_pc = configure_file(
	input: 'libgda-ui-'+project_api_version+'.pc.in',
	output: 'libgda-ui-'+project_api_version+'.pc',
	configuration: conf,
	install: true,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)
libgda_report_pc = configure_file(
	input: 'libgda-report-'+project_api_version+'.pc.in',
	output: 'libgda-report-'+project_api_version+'.pc',
	configuration: conf,
	install: true,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)
endif

subdir('tools')
subdir('tests')
subdir('testing')

