## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

lib_LTLIBRARIES = libgda_sql_transaction-3.0.la
noinst_PROGRAMS = sql_parser_test

libgda_sql_transaction_3_0_la_SOURCES = \
	parser.c \
	parser.h \
	lexer.c \
	gda-sql-transaction-parser.c \
	gda-sql-transaction-parser.h \
	gda-sql-transaction-tree.c \
	gda-sql-transaction-tree.h

libgda_sql_transaction_3_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_sql_transaction_3_0_la_LIBADD = $(LIBGDA_LIBS)

parser.c: parser.y
	$(YACC) -v -d -o parser.c -p tran $<

lexer.c: lexer.l parser.c
	$(LEX) -olexer.c -Ptran $<

libsqltransactionincdir = $(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/libsql

sql_parser_test_SOURCES=sql_parser_test.c
sql_parser_test_DEPENDENCIES = libgda_sql_transaction-3.0.la
sql_parser_test_LDADD = libgda_sql_transaction-3.0.la $(LIBGDA_LIBS)

EXTRA_DIST=parser.y lexer.l
CLEANFILES = lexer.c parser.h parser.c parser.output
