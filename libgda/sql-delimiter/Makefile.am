noinst_LTLIBRARIES = libgda_sql_delimiter-3.0.la
noinst_PROGRAMS = gda-test-delimiter

SUBDIRS = 

AM_CPPFLAGS = \
	-I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) 

parser.c: parser.y
	$(YACC) -v -d -o $@ -p gda_delimiter $<

lexer.c: lexer.l parser.c
	$(LEX) -o $@ -Pgda_delimiter $<

libgda_sql_delimiter_headers = \
        gda-sql-delimiter.h 

libgda_sql_delimiterincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/sql-delimiter
libgda_sql_delimiterinclude_HEADERS=$(libgda_sql_delimiter_headers)

libgda_sql_delimiter_3_0_la_SOURCES = \
	$(libgda_sql_delimiter_headers) \
	parser.c \
        lexer.c \
        parser.h \
        gda-sql-delimiter.c \
	gda-delimiter-tree.h \
        gda-delimiter-tree.c

EXTRA_DIST=parser.y lexer.l

gda_test_delimiter_SOURCES = gda-test-delimiter.c
gda_test_delimiter_LDADD = libgda_sql_delimiter-3.0.la $(LIBGDA_LIBS)

CLEANFILES = parser.h parser.c lexer.c parser.output
