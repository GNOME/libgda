lemon_exe = executable('lemon','lemon.c')
gendef_exe = executable('gen_def','gen_def.c')

lemon_parser = custom_target('lemon-parser',
    input : ['lempar.c', 'parser.y'],
    output : ['parse.c', 'parser.h'],    
    command : [lemon_exe, '-d',
    '-T@INPUT0@ @INPUT1@'],
    install : false
    )

lemon_delimiter = custom_target('lemon-delimiter',
    input : ['lempar.c', 'delimiter.y'],
    output : ['delimiter.c', 'delimiter.h'],    
    command : [lemon_exe, '-q','-d',
    '-T@INPUT0@ @INPUT1@'],
    install : false
    )

run_target('gendef', 
            command: [gendef_exe, '>', 'token_types.h'],
            depends: gendef_exe)

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

mkenums_out = gnome_module.mkenums('s-enum-types-c',
    fhead : '''#include <sql-parser/gda-sql-parser.h>
    #include <sql-parser/gda-sql-statement.h>
    #include <glib-object.h>''',
    vhead : '''GType
        @enum_name@_get_type (void)
        {
          static GType etype = 0;
            if (etype == 0) {
                static const G@Type@Value values[] = {''',
    vprod : '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },', 
    vtail : '''      { 0, NULL, NULL }
        };
            etype = g_@type@_register_static ("@EnumName@", values);
              }
                return etype;
                }''',
    sources : libgda_sql_parser_headers
    )


