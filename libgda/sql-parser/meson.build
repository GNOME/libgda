lemonf = files([
	'lemon.c'
	])

lemon_exe = executable('lemon', lemonf, install: false)

libgda_sql_parser_headers = [
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
	'gda-statement-struct-util.h']

gda_sql_parser_enums = gnome_module.mkenums_simple('gda-sql-parser-enum-types', sources: libgda_sql_parser_headers)

gendef_exe = executable('gen_def', 'gen_def.c',
	c_args: [
		'-DSRCDIR="'+join_paths (meson.source_root(), 'libgda', 'sql-parser')+'"',
		'-DBUILDDIR="'+meson.current_build_dir()+'"'
		]
	)

lemon_parserc = custom_target('lemon-parser_c',
	command : [lemon_exe, '-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : ['lempar.c', 'parser.y'],
	output : ['parser.c'],
	install : false
	)


lemon_parserh = custom_target('lemon-parser_h',
	command : [lemon_exe, '-o'+meson.current_build_dir(), '-h',
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : ['lempar.c', 'parser.y'],
	output : ['parser.h'],
	install : false
	)

lemon_delimiterc = custom_target('lemon-delimiter_c',
	command : [lemon_exe, '-q',
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : ['lempar.c', 'delimiter.y'],
	output : ['delimiter.c'],
	install : false
	)

lemon_delimiterh = custom_target('lemon-delimiter_h',
	command : [lemon_exe, '-q',
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : ['lempar.c', 'delimiter.y'],
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

libgda_sql_parser_sources = files ([
	'gda-sql-parser-private.h',
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

libgda_sql_parser_sources += [
			libgda_sql_parser_headers,
			gda_sql_parser_enums,
			lemon_parserc,
			lemon_parserh,
			lemon_delimiterc,
			lemon_delimiterh,
			token_types
			]
libgda_sql_parser = library(
	'gda_sql_parser',
	[libgda_sql_parser_sources],
	dependencies: [glib_dep, inc_rooth_dep, inc_sqliteh_dep],
	c_args : [
		'-include',
		meson.build_root() + '/config.h',
	],
	install: false
	)

