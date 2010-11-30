noinst_LTLIBRARIES = libgda-test-4.0.la
TESTS = test-ddl-creator test-bin-converter test-sql-identifier test-identifiers-quotes test-sql-builder test-connection-string-split test-input-parsers
check_PROGRAMS = test-ddl-creator test-bin-converter test-sql-identifier test-identifiers-quotes test-sql-builder test-connection-string-split test-input-parsers


SUBDIRS = providers parser value-holders meta-store data-models multi-threading

AM_CPPFLAGS = \
        -I$(top_builddir) \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        $(LIBGDA_CFLAGS) \
	-DCHECK_FILES=\""$(top_srcdir)"\" \
	-DTOP_BUILD_DIR=\""$(top_builddir)"\"

test_headers = \
        gda-ddl-creator.h

libgda_test_4_0_la_SOURCES = \
        $(test_headers) \
        gda-ddl-creator.c \
	test-cnc-utils.h \
	test-cnc-utils.c \
	data-model-errors.h \
	data-model-errors.c

libgda_test_4_0_la_LDFLAGS = $(NO_UNDEFINED)

libgda_test_4_0_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-4.0.la 

test_ddl_creator_SOURCES = \
        test-ddl-creator.c

test_ddl_creator_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
	libgda-test-4.0.la \
        $(LIBGDA_LIBS)


test_bin_converter_SOURCES = \
        test-bin-converter.c

test_bin_converter_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
	libgda-test-4.0.la \
        $(LIBGDA_LIBS)

test_sql_identifier_SOURCES = \
        test-sql-identifier.c

test_sql_identifier_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

test_identifiers_quotes_SOURCES = \
        test-identifiers-quotes.c

test_identifiers_quotes_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

test_sql_builder_SOURCES = \
        test-sql-builder.c

test_sql_builder_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

test_connection_string_split_SOURCES = \
        test-connection-string-split.c

test_connection_string_split_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

test_input_parsers_SOURCES = \
        test-input-parsers.c

test_input_parsers_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = dbstruct.xml
