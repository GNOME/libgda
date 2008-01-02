noinst_LTLIBRARIES = libgda_sql_parser-3.0.la
noinst_PROGRAMS = 

AM_CPPFLAGS = \
	-I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS)

lemon$(EXEEXT_FOR_BUILD): lemon.c
	$(CC_FOR_BUILD) -o lemon$(EXEEXT_FOR_BUILD) lemon.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	 $(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) gen_def.c

parser.c parser.h: parser.y lemon$(EXEEXT_FOR_BUILD)
	- ./lemon$(EXEEXT_FOR_BUILD) parser.y

delimiter.c delimiter.h: delimiter.y lemon$(EXEEXT_FOR_BUILD)
	./lemon$(EXEEXT_FOR_BUILD) delimiter.y

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


$(OBJECTS) $(libgda_sql_parser_3_0_la_OBJECTS): token_types.h
libgda_sql_parserincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/sql-parser
libgda_sql_parserinclude_HEADERS=$(libgda_sql_parser_headers)

libgda_sql_parser_3_0_la_SOURCES = \
	$(libgda_sql_parser_headers) \
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

EXTRA_DIST= parser.y delimiter.y lemon.c lempar.c gen_def.c

CLEANFILES = parser.h parser.c parser.out delimiter.h delimiter.c delimiter.out token_types.h \
	lemon$(EXEEXT_FOR_BUILD) gen_def$(EXEEXT_FOR_BUILD)
