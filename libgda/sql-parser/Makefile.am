noinst_LTLIBRARIES = libgda_sql_parser-5.0.la
noinst_PROGRAMS = 

AM_CPPFLAGS = \
	-I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGDS)

lemon$(EXEEXT_FOR_BUILD): lemon.c
	$(CC_FOR_BUILD) -o lemon$(EXEEXT_FOR_BUILD) $(srcdir)/lemon.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	 $(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DSRCDIR=\"$(srcdir)\" $(srcdir)/gen_def.c

parser.c parser.h: parser.y lemon$(EXEEXT_FOR_BUILD)
	- ./lemon$(EXEEXT_FOR_BUILD) -d -T$(srcdir)/lempar.c $(srcdir)/parser.y

delimiter.c delimiter.h: delimiter.y lemon$(EXEEXT_FOR_BUILD)
	./lemon$(EXEEXT_FOR_BUILD) -q -d -T$(srcdir)/lempar.c $(srcdir)/delimiter.y

token_types.h: gen_def$(EXEEXT_FOR_BUILD) delimiter.h parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > token_types.h

libgda_sql_parser_headers = \
	gda-sql-parser.h \
	gda-sql-statement.h \
	gda-statement-struct-decl.h \
	gda-statement-struct.h \
	gda-statement-struct-compound.h \
	gda-statement-struct-delete.h \
	gda-statement-struct-update.h \
	gda-statement-struct-insert.h \
	gda-statement-struct-parts.h \
	gda-statement-struct-pspec.h \
	gda-statement-struct-select.h \
	gda-statement-struct-trans.h \
	gda-statement-struct-unknown.h \
	gda-statement-struct-util.h

gda-sql-parser-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(libgda_sql_parser_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#ifndef __LIBGDA_SQL_PARSER_ENUM_TYPES_H__\n#define __LIBGDA_SQL_PARSER_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDA_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBGDA_ENUM_TYPES_H__ */" \
		$(libgda_sql_parser_headers)) > tmp-gda-enum-types.h \
	&& (cmp -s tmp-gda-enum-types.h gda-sql-parser-enum-types.h || cp tmp-gda-enum-types.h gda-sql-parser-enum-types.h ) \
	&& rm -f tmp-gda-enum-types.h	\
	&& echo timestamp > $(@F)

gda-sql-parser-enum-types.c: s-enum-types-c
	@true

s-enum-types-c: @REBUILD@ $(libgda_sql_parser_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#include <sql-parser/gda-sql-parser.h>\n" \
	                --fhead "#include <sql-parser/gda-sql-statement.h>\n" \
	                --fhead "#include <glib-object.h>" \
                        --fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(libgda_sql_parser_headers)) > tmp-gda-enum-types.c \
	&& (cmp -s tmp-gda-enum-types.c gda-sql-parser-enum-types.c || cp tmp-gda-enum-types.c gda-sql-parser-enum-types.c ) \
	&& rm -f tmp-gda-enum-types.c \
	&& echo timestamp > $(@F)

# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
libgda_sql_parser_built_headers = gda-sql-parser-enum-types.h
libgda_sql_parser_built_cfiles = gda-sql-parser-enum-types.c

$(OBJECTS) $(libgda_sql_parser_5_0_la_OBJECTS): token_types.h
libgda_sql_parserincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/sql-parser
libgda_sql_parserinclude_HEADERS=$(libgda_sql_parser_headers) $(libgda_sql_parser_built_headers)

libgda_sql_parser_5_0_la_SOURCES = \
	$(libgda_sql_parser_headers) \
	$(libgda_sql_parser_built_headers) \
	$(libgda_sql_parser_built_cfiles) \
	gda-sql-parser-private.h \
	parser.c \
        parser.h \
	delimiter.c \
	delimiter.h \
	token_types.h \
        gda-sql-parser.c \
        gda-statement-struct.c \
	gda-statement-struct-compound.c \
        gda-statement-struct-delete.c \
        gda-statement-struct-update.c \
        gda-statement-struct-insert.c \
        gda-statement-struct-parts.c \
        gda-statement-struct-pspec.c \
        gda-statement-struct-select.c \
        gda-statement-struct-trans.c \
        gda-statement-struct-unknown.c \
        gda-statement-struct-util.c

EXTRA_DIST= parser.y delimiter.y lemon.c lempar.c gen_def.c parser_tokens.h

CLEANFILES = parser.h parser.c parser.out delimiter.h delimiter.c delimiter.out token_types.h \
	lemon$(EXEEXT_FOR_BUILD) gen_def$(EXEEXT_FOR_BUILD) \
	$(libgda_sql_parser_built_headers) $(libgda_sql_parser_built_cfiles) \
	gda-enum-types.h \
	s-enum-types-h \
	gda-enum-types.c \
	s-enum-types-c
