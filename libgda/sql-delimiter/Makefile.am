lib_LTLIBRARIES = libgda_sql_delimiter-3.la
bin_PROGRAMS = gda-test-delimiter

SUBDIRS = 

AM_CPPFLAGS = \
	-I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) 

parser.c: parser.y
	$(YACC) -v -d -o parser.c -p gda_delimiter parser.y

lexer.c: lexer.l parser.c
	$(LEX) -olexer.c -Pgda_delimiter lexer.l

libgda_sql_delimiter_headers = \
        gda-sql-delimiter.h 

libgda_sql_delimiterincludedir=$(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/sql-delimiter
libgda_sql_delimiterinclude_HEADERS=$(libgda_sql_delimiter_headers)

libgda_sql_delimiter_3_la_SOURCES = \
	$(libgda_sql_delimiter_headers) \
	parser.c \
        lexer.c \
        parser.h \
        gda-sql-delimiter.c \
	gda-delimiter-tree.h \
        gda-delimiter-tree.c

libgda_sql_delimiter_3_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)

EXTRA_DIST=parser.y lexer.l

gda_test_delimiter_SOURCES = gda-test-delimiter.c
gda_test_delimiter_LDADD = libgda_sql_delimiter-3.la $(LIBGDA_LIBS)

CLEANFILES = parser.h parser.c lexer.c parser.output
