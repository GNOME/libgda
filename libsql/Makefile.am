## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

parser.c: $(top_srcdir)/libsql/parser.y
	$(YACC) -v -d -o parser.c -p sql $<

lexer.c: $(top_srcdir)/libsql/lexer.l parser.c
	$(LEX) -olexer.c -Psql $<

noinst_PROGRAMS = sql_parser_test
lib_LTLIBRARIES = libgdasql-3.0.la

libgdasql_3_0_la_SOURCES = \
	parser.c \
	parser.h \
	lexer.c \
	sql_parser.c \
	sql_parser.h \
	mem.c \
	mem.h \
	sql_display.c \
	sql_tree.c \
	sql_tree.h
libgdasqlincdir = $(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libsql
libgdasqlinc_HEADERS = sql_parser.h
libgdasql_3_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgdasql_3_0_la_LIBADD = $(LIBGDA_LIBS)

sql_parser_test_SOURCES=sql_parser_test.c
sql_parser_test_DEPENDENCIES = libgdasql-3.0.la
sql_parser_test_LDADD = libgdasql-3.0.la $(LIBGDA_LIBS)

tests: sql_parser_test

EXTRA_DIST=parser.y lexer.l
CLEANFILES = lexer.c parser.h parser.c parser.output
