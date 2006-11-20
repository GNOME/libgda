## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

lib_LTLIBRARIES = libsqltransaction.la
noinst_PROGRAMS = sql_parser_test

libsqltransaction_la_SOURCES = \
	parser.c \
	parser.h \
	lexer.c \
	gda-sql-transaction-parser.c \
	gda-sql-transaction-parser.h \
	gda-sql-transaction-tree.c \
	gda-sql-transaction-tree.h

parser.c: parser.y
	$(YACC) -v -d -o parser.c -p tran $<

lexer.c: lexer.l parser.c
	$(LEX) -olexer.c -Ptran $<

libsqltransactionincdir = $(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/libsql

sql_parser_test_SOURCES=sql_parser_test.c
sql_parser_test_DEPENDENCIES = libsqltransaction.la
sql_parser_test_LDADD = libsqltransaction.la $(LIBGDA_LIBS)

EXTRA_DIST=parser.y lexer.l
CLEANFILES = lexer.c parser.h parser.c parser.output
