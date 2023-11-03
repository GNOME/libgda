project('libgda', 'c',
        license: 'LGPL2+',
        version : '6.0.1',
        meson_version: '>= 0.58',
        default_options: [
                        'c_std=gnu11',
                        'warning_level=2',
                        'werror=true'
                        ],
)

enable_vala = get_option('vapi')
if enable_vala
	enable_vala = add_languages('vala')
endif

i18n = import('i18n')
intlmerge = find_program('intltool-merge', required: false)

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

# LT_VERSION for ABI related changes
# From: https://autotools.io/libtool/version.html
# This rules applies to Meson 0.43
# If interfaces have been changed or added, set GDA_CURRENT += 1 and GDA_AGE += 1, set GDA_REVISION to 0.
# If binary compatibility has been broken (e.g. removed or changed interfaces), set GDA_CURRENT += 1, GDA_REVISION and GDA_AGE to 0
# If interfaces is the same as the previous version, set GDA_REVISION += 1
# Set version to the value of subtract age from current
# Reset current and version to 1 and, age and version to 0 if library's name is changed
LT_CURRENT='6'
LT_REVISION='0'
LT_AGE='0'
LT_VERSION='6'

compiler = meson.get_compiler('c')

# Use this variables to access top source and top build dirs when used as subproject
gda_top_src = meson.current_source_dir()
gda_top_build = meson.current_build_dir()

glib_genmarshal = find_program('glib-genmarshal', required : true)

glib_mkenums = find_program('glib-mkenums', required : true)

girscanner = find_program('g-ir-scanner', required:false)

# Meson does not currently provide built-in support for valadoc [1],
# we have to find the program, trying all possible names.
# It also makes it impossible for us to use gnome.generate_vapi() because
# the generated vapi files are inputs to valadoc, hence we need to find vapigen.
# [1] https://github.com/mesonbuild/meson/issues/894
valadoc = false
vapigen = false
if enable_vala
	vala_version = meson.get_compiler('vala').version().split('.')
	valadoc_versioned = 'valadoc-' + '.'.join([vala_version[0], vala_version[1]])
	valadoc = find_program(valadoc_versioned, 'valadoc', required: false)
	vapigen_versioned = 'vapigen-' + '.'.join([vala_version[0], vala_version[1]])
	vapigen = find_program(vapigen_versioned, 'vapigen', required: true)
endif

libgda_inc_rooth = include_directories ('.')
inc_rooth_dep = declare_dependency (include_directories : libgda_inc_rooth)

req_ver_glib = '>= 2.38.0'
glib_dep = dependency('glib-2.0', version : req_ver_glib)
gio_dep = dependency('gio-2.0', version : '>=2.16')
gobject_dep = dependency('gobject-2.0', version : req_ver_glib)
gthread_dep = dependency('gthread-2.0', version : req_ver_glib)
gmodule_no_export_dep = dependency('gmodule-no-export-2.0', version : req_ver_glib)
libxml_dep = dependency('libxml-2.0')

goocanvas_dep = dependency('goocanvas-2.0', required: get_option('goocanvas'))
graphviz_dep = dependency('libgvc', required : false)
libxslt_dep = dependency('libxslt', required: false)
jsonglib_dep = dependency('json-glib-1.0',required : false)
postgres_dep = dependency ('libpq', required: false)
mysql_dep = dependency ('mysqlclient', required: false)
mysql_args = []
if mysql_dep.found ()
if mysql_dep.version ().version_compare ('>21.0.0')
mysql_args += [
	'-DMYSQL8'
	]
endif
endif

# Platform data
socket_dep = []
windows = build_machine.system().contains('windows')
if windows
win = import('windows')
socket_dep = meson.get_compiler('c').find_library('ws2_32')
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

sqlite_dep = dependency ('sqlite3', version: '>= 3.22.0', required: true)
if sqlite_dep.found()
	if compiler.has_function('sqlite3_table_column_metadata', dependencies: sqlite_dep)
		message('Installed SQLite was not compiled with the SQLITE_ENABLE_COLUMN_METADATA')
	endif
endif

sqlcipher_dep = dependency ('sqlcipher', version: '>= 3.4', required: false)
if sqlcipher_dep.found() and not get_option('sqlcipher')
	sqlcipher_table_column_metadata = compiler.has_function('sqlite3_table_column_metadata',
								dependencies: sqlcipher_dep)
endif

sed = find_program('sed')
compile_resources = find_program('glib-compile-resources')

libgda_external_dep = [
	glib_dep,
	gobject_dep,
	gio_dep,
	libxml_dep,
	inc_rooth_dep,
        socket_dep
	]
libgda_dep = [
	gmodule_no_export_dep,
	]
libgda_dep += libgda_external_dep

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
enable_ldap = get_option('ldap') and ldap_found and get_option('experimental')

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
conf.set('GDA_ABI_MAJOR_VERSION', API_MAJOR_VERSION)
conf.set('GDA_ABI_MINOR_VERSION', API_MINOR_VERSION)
conf.set('VERSION', project_version)
conf.set('prefix', get_option('prefix'))
conf.set('exec_prefix', join_paths (get_option('prefix'), get_option('libexecdir')))
conf.set('libdir', join_paths (get_option('prefix'), get_option('libdir')))
conf.set('includedir', join_paths (get_option('prefix'), get_option('includedir')))
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

general_cargs = [
	'-DLIBGDA_ABI_NAME='+project_api_name
	]
c_args = ['-Wall',
	'-Wextra',
	'-Wformat-security',
	'-Wno-address',
	'-Wno-unused-variable',
	'-Wno-unused-parameter',
	'-Wno-pointer-sign',
	'-Wno-missing-field-initializers',
	'-DGSEAL_ENABLE',
	'-DG_LOG_USE_STRUCTURED'
	]

c_args += general_cargs

glade_catalog = ''
glade_pixmap = ''

gnome_module = import('gnome')

if get_option('json')
  jsonglib_dep = dependency('json-glib-1.0')
endif


iso_codes_dep = dependency('iso-codes')
icodes = iso_codes_dep.get_variable('domains')
iso_codes_prefix = ''
if icodes.contains('4217')
iso_codes_prefix = iso_codes_dep.get_variable('prefix')
endif
iso_codes_cargs = [
	'-DISO_CODES_PREFIX="'+iso_codes_prefix+'"'
	]

libgcrypt_config = find_program('libgcrypt-config', required : false)
if libgcrypt_config.found()
  conf.set('HAVE_LIBGCRYPT', 1)
endif

libcrypto_dep = dependency('libcrypto', version : '>=1.1', required: false)

soup_dep = dependency('libsoup-2.4', required: false)
if soup_dep.found() and get_option('libsoup')
  libgda_dep += [soup_dep]
endif


libsecret_dep = dependency('libsecret-1', required: false)
if libsecret_dep.found() and get_option('libsecret')
  libgda_dep += [libsecret_dep]
  c_args += '-DHAVE_LIBSECRET'
endif

build_type = get_option('buildtype')

if get_option('debug') or ('debug' == build_type) or ('debugoptimized' == build_type)
  c_args += '-DGDA_DEBUG'
endif

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
	'sqlite_specs_drop_view',
	'sqlite_specs_rename_column'
	]
sqlite_xml_fnamesext = []
foreach fn : sqlite_xml_fnames
	sqlite_xml_fnamesext += [join_paths(meson.current_source_dir(), 'providers','sqlite',fn+'.xml.in')]
endforeach

sqlite_xml_inf = files(sqlite_xml_fnamesext)

found_jdbc = false

dates = compiler.run('''
#include<glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>

void main (void) {
  GDateTime *dt;
  char *source_date_epoch;
  gint64 epoch;
  /* This assumes that the SOURCE_DATE_EPOCH environment variable will contain
     a correct, positive integer */
  if ((source_date_epoch = getenv("SOURCE_DATE_EPOCH")) == NULL ||
    (epoch = (time_t)strtoll(source_date_epoch, NULL, 10)) <= 0 ||
    (dt = g_date_time_new_from_unix_utc(epoch)) == NULL)
       dt = g_date_time_new_now_utc ();
  g_print (g_date_time_format(dt, "%F"));
}
''',
dependencies: glib_dep
)

subdir('po')
subdir('libgda')
subdir('providers')

gtk_dep = dependency('gtk+-3.0', required: false)
enable_ui = get_option('ui') and gtk_dep.found() and get_option('experimental') and intlmerge.found()
if enable_ui
	gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')
	glade_dep = dependency('gladeui-2.0', required : get_option('glade'))
	if goocanvas_dep.found()
		math_dep = compiler.find_library('m')
	endif
	gtksourceview_dep = dependency('gtksourceview-3.0', required: get_option('gtksourceview'))
	graphviz_dep = dependency('libgvc', required: false)
	graphviz_cargs = []
	if get_option('graphviz') and graphviz_dep.found()
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
                option_glade_catalog = get_option('glade-catalog-dir')
                option_glade_pixmap = get_option('glade-pixmap-dir')
                if option_glade_catalog != ''
                        glade_catalog = option_glade_catalog
                else
		        glade_catalog = glade_dep.get_variable('catalogdir')
                endif

                if option_glade_pixmap != ''
                        glade_pixmap = option_glade_pixmap
                else
		        glade_pixmap = glade_dep.get_variable('pixmapdir')
                endif
	endif
	subdir('libgda-ui')
	subdir('libgda-report')
	windows_module = import('windows')
	subdir('control-center')
endif

if get_option('doc')
        subdir('doc')
endif

if libxslt_dep.found() and get_option('experimental')
	subdir('libgda-xslt')
endif

# PC files
libgda_pc = configure_file(
	input: 'libgda-'+project_api_version+'.pc.in',
	output: 'libgda-'+project_api_version+'.pc',
	configuration: conf
	)
install_data(libgda_pc,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)

if libxslt_dep.found() and get_option('experimental')
	libgda_xslt_pc = configure_file(
		input: 'libgda-xslt-'+project_api_version+'.pc.in',
		output: 'libgda-xslt-'+project_api_version+'.pc',
		configuration: conf
		)
	install_data(libgda_xslt_pc,
		install_dir: join_paths(get_option('libdir'),'pkgconfig')
		)
endif

if enable_ui
libgdaui_pc = configure_file(
	input: 'libgda-ui-'+project_api_version+'.pc.in',
	output: 'libgda-ui-'+project_api_version+'.pc',
	configuration: conf
	)
install_data(libgdaui_pc,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)
libgda_report_pc = configure_file(
	input: 'libgda-report-'+project_api_version+'.pc.in',
	output: 'libgda-report-'+project_api_version+'.pc',
	configuration: conf
	)
install_data(libgda_report_pc,
	install_dir: join_paths(get_option('libdir'),'pkgconfig')
	)
endif

if get_option('experimental') and  get_option('tools')
subdir('tools')
endif
subdir('tests')
subdir('testing')
if get_option('examples')
  subdir('examples')
endif
