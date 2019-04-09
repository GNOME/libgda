inc_libsql_parserh = include_directories ('.')
inc_libsql_parserh_dep = declare_dependency (include_directories : inc_libsql_parserh)

lemonf = files([
	'lemon.c'
	])

lemon_exe = executable('lemon',
	lemonf,
	dependencies: [gio_dep],
	install: false
)

libgda_sql_parser_headers = files([
	'gda-statement-struct-decl.h',
	'gda-sql-statement.h',
	'gda-sql-parser.h',
	'gda-statement-struct.h',
	'gda-statement-struct-compound.h',
	'gda-statement-struct-delete.h',
	'gda-statement-struct-update.h',
	'gda-statement-struct-insert.h',
	'gda-statement-struct-parts.h',
	'gda-statement-struct-pspec.h',
	'gda-statement-struct-select.h',
	'gda-statement-struct-trans.h',
	'gda-statement-struct-unknown.h',
	'gda-statement-struct-util.h'])

install_headers(libgda_sql_parser_headers,
	install_dir: join_paths(get_option('includedir'),project_package,'libgda','sql-parser')
	)

gda_sql_parser_enums = gnome_module.mkenums_simple('gda-sql-parser-enum-types', sources: libgda_sql_parser_headers)

gendef_exe = executable('gen_def', 'gen_def.c',
	c_args: [
		'-DSRCDIR="'+meson.current_source_dir()+'"',
		'-DBUILDDIR="'+meson.current_build_dir()+'"'
		]
	)

lemon_templatef = files([ 'lempar.c' ])

lemon_parserc = custom_target('lemon-parser_c',
	command : [lemon_exe, '-d', '-h',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.c'],
	install : false
	)


lemon_parserh = custom_target('lemon-parser_h',
	command : [lemon_exe, '-d', '-z',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.h'],
	install : false
	)

lemon_delimiterc = custom_target('lemon-delimiter_c',
	command : [lemon_exe, '-q', '-d', '-h',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'delimiter.y'],
	output : ['delimiter.c'],
	install : false
	)

lemon_delimiterh = custom_target('lemon-delimiter_h',
	command : [lemon_exe, '-q', '-d', '-z',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'delimiter.y'],
	output : ['delimiter.h'],
	install : false
	)

token_types = custom_target('token_types',
	command: [gendef_exe],
	capture: true,
	input: [lemon_parserh, lemon_delimiterh],
	output: 'token_types.h',
	install: false
	)
token_types_dep = declare_dependency(sources: token_types)


# mkenums_out = gnome_module.mkenums('s-enum-types-c',
#     fhead : '''#include <sql-parser/gda-sql-parser.h>
    #include <sql-parser/gda-sql-statement.h>
    #include <glib-object.h>''',
#     vhead : '''GType
#         @enum_name@_get_type (void)
#         {
#           static GType etype = 0;
#             if (etype == 0) {
#                 static const G@Type@Value values[] = {''',
#     vprod : '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
#     vtail : '''      { 0, NULL, NULL }
#         };
#             etype = g_@type@_register_static ("@EnumName@", values);
#               }
#                 return etype;
#                 }''',
#     sources : libgda_sql_parser_headers
#     )

sql_parser_private_sources = files ([
	'gda-sql-parser-private.h',
	])


sql_parser_sources = files ([
	'gda-sql-parser.c',
	'gda-statement-struct.c',
	'gda-statement-struct-compound.c',
	'gda-statement-struct-delete.c',
	'gda-statement-struct-update.c',
	'gda-statement-struct-insert.c',
	'gda-statement-struct-parts.c',
	'gda-statement-struct-pspec.c',
	'gda-statement-struct-select.c',
	'gda-statement-struct-trans.c',
	'gda-statement-struct-unknown.c',
	'gda-statement-struct-util.c'
	])

sql_parser_gi_sources = []
sql_parser_gi_sources += [
	sql_parser_sources,
	libgda_sql_parser_headers
	]

libgda_sql_parser_sources = []
libgda_sql_parser_sources += [
			sql_parser_private_sources,
			sql_parser_sources,
			libgda_sql_parser_headers,
			gda_sql_parser_enums,
			lemon_parserc,
			lemon_parserh,
			lemon_delimiterc,
			lemon_delimiterh
			]

