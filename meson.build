project('libgda', 'c',
        license: 'LGPL2+',
        version : '5.99.0',
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
project_api_version = '6.0'
project_abi_version = MAJOR_VERSION+'.'+MINOR_VERSION
project_abi_name = '"' + meson.project_name() + '-' + project_abi_version + '"'
project_package = meson.project_name() + '-' + project_api_version

gda_current = '5'
gda_revision ='1'
gda_age = '1'

conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
conf.set_quoted('GETTEXT_PACKAGE', project_package)
conf.set_quoted('PACKAGE_VERSION', project_version)


if meson.get_compiler('c').has_header('locale.h')
	conf.set ('HAVE_LOCALE_H', '1')
endif

glib_genmarshal = find_program('glib-genmarshal', required : true)
if glib_genmarshal.found()
  conf.set('HAVE_GLIB_GENMARSHAL',1)
else
  error ('glib-genmarshal not found')
endif

glib_mkenums = find_program('glib-mkenums', required : true)
if glib_mkenums.found()
  conf.set('HAVE_GLIB_MKENUMS',1)
else
  error ('glib-mkenums not found')
endif

configh = configure_file(output : 'config.h',
			configuration : conf)


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
glade_dep = dependency('gladeui-2.0', required : false)
libxslt_dep = dependency('libxslt')
jsonglib_dep = dependency('json-glib-1.0',required : false)
sqlite_dep = dependency ('sqlite3', version: '>= 3.6.11')
postgres_dep = dependency ('libpq', required: false)


libgda_dep = [
	glib_dep,
	gobject_dep,
	gio_dep,
	gmodule_no_export_dep,
	libxml_dep,
	inc_rooth_dep
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
	'-DLIBGDA_ABI_NAME='+project_abi_name]

glade_catalog = ''
glade_pixmap = ''

pkg_module = import('pkgconfig')
gnome_module = import('gnome')

if get_option('with-ui')
  gtk_dep = dependency('gtk+-3.0')
  gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')

  if get_option('with-goocanvas')
    goocanvas_dep = dependency('goocanvas-2.0')
  endif
  
  if get_option('with-gtksourceview')
    gtksourceview_dep = dependency('gtksourceview-3.0')
  endif

  if get_option('with-graphviz')
    graphviz_dep = dependency('libgvc')
    graph_new_api_code = '''#include <gvc.h>
int main() {
    Agraph_t *graph;
    graph = agopen ("BrowserCanvasLayout", Agdirected, NULL);
    return 0;
}    
'''
    graph_new_api_code_results = compiler.compiles(graph_new_api_code)
    if graph_new_api_code_results
      conf.set('HAVE_GRAPHVIZ_NEW_API',1)
    endif
  endif
  
  if glade_dep
    glade_catalog = pkg_module.generate(libraries: 'gladeui-2.0', variables : 'catalogdir')
    glade_catalog = pkg_module.generate(libraries: 'gladeui-2.0', variables : 'pixmapdir')    
  endif  
endif

if get_option('enable-json')
  jsonglib_dep = dependency('json-glib-1.0')
endif


# ******************************
# Checks for iso codes
# ******************************
#isocodes_prefix = ''

#iso_codes = pkg_module.generate(libraries: 'iso-codes', variables : 'domains')
#if (iso_codes.find('4217'))
#  isocodes_prefix = pkg_module.generate(libraries: 'iso-codes', variables : 'prefix') 
#endif
### This is original code from configure.ac. It should be converted to meson.
#ISO_CODES_MODULES="iso-codes"
#PKG_CHECK_MODULES(ISO_CODES, $ISO_CODES_MODULES, have_iso_codes=yes, have_iso_codes=no)
#if test x"$have_iso_codes" = "xyes"
#then
#        AC_MSG_CHECKING([whether iso-codes has iso-4217 domain])
#        if $PKG_CONFIG --variable=domains iso-codes | grep -q 4217 ; then
#                AC_MSG_RESULT([yes])
#                AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
#        else
#                AC_MSG_RESULT([no])
#                AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],"")
#        fi
#else
#        AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],"")
#fi

libcryp_config = find_program('libcryp-config', required : false)
if libcryp_config.found()
  conf.set('HAVE_LIBCRYPT_CONFIG', 1)
endif

if get_option('with-libsoup')
  soup_dep = dependency('libsoup-2.4')
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

subdir('po')
subdir('libgda')
subdir('providers')
#subdir('libgda-ui')







