## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

parser.c: parser.y
	$(YACC) -v -d -o parser.c -p sql parser.y

lexer.c: lexer.l parser.c
	$(LEX) -olexer.c -Psql lexer.l

noinst_PROGRAMS = sql_parser_test
lib_LTLIBRARIES = libgdasql.la

libgdasql_la_SOURCES = \
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
libgdasqlincdir = $(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/sql
libgdasqlinc_HEADERS = sql_parser.h
libgdasql_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(LIBGDA_LIBS)

sql_parser_test_SOURCES=sql_parser_test.c
sql_parser_test_LDFLAGS = $(LIBGDA_LIBS)
sql_parser_test_DEPENDENCIES = libgdasql.la
sql_parser_test_LDADD = libgdasql.la

tests: sql_parser_test

EXTRA_DIST=parser.y lexer.l
