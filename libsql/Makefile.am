## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

parser.c: parser.y
	$(YACC) -v -d -o parser.c -p sql parser.y

lexer.c: lexer.l parser.c
	$(LEX) -olexer.c -Psql lexer.l

noinst_PROGRAMS = sql_parse_test

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
libgdasqlincdir = $(includedir)/libgda/sql
libgdasqlinc_HEADERS = 
libgdasql_la_LDFLAGS = $(LIBGDA_LIBS)

sql_parse_test_SOURCES=sql_parse_test.c
sql_parse_test_LDFLAGS = $(LIBGDA_LIBS)
sql_parse_test_DEPENDENCIES =
sql_parse_test_LDADD = $(top_builddir)/libsql/libgdasql.la

tests: sql_parse_test

EXTRA_DIST=parser.y lexer.l
