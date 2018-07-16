noinst_LTLIBRARIES = libgda-test-6.0.la

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
TESTS = test-ddl-creator test-bin-converter test-sql-identifier test-identifiers-quotes test-sql-builder test-connection-string-split test-input-parsers test-quark-list test-sql-renderer test-server-operation
check_PROGRAMS = test-ddl-creator test-bin-converter test-sql-identifier test-identifiers-quotes test-sql-builder test-connection-string-split test-input-parsers test-quark-list test-sql-renderer test-server-operation

if HAVE_UI
UI_EXTENSION = ui
endif

SUBDIRS = providers parser value-holders meta-store data-models multi-threading ddl $(UI_EXTENSION) $(VALA_EXTENSIONS)

AM_CPPFLAGS = \
        -I$(top_builddir) \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        $(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DCHECK_FILES=\""$(top_srcdir)"\" \
	-DTOP_BUILD_DIR=\""$(top_builddir)"\"

test_headers = \
        raw-ddl-creator.h

libgda_test_6_0_la_SOURCES = \
        $(test_headers) \
	test-errors.h \
	test-errors.c \
        raw-ddl-creator.c \
	test-cnc-utils.h \
	test-cnc-utils.c \
	data-model-errors.h \
	data-model-errors.c

libgda_test_6_0_la_LDFLAGS = $(NO_UNDEFINED)

libgda_test_6_0_la_LIBADD = \
	$(COREDEPS_LIBS) \
	$(top_builddir)/libgda/libgda-6.0.la 

test_ddl_creator_SOURCES = \
        test-ddl-creator.c

test_ddl_creator_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
	libgda-test-6.0.la \
        $(COREDEPS_LIBS)


test_bin_converter_SOURCES = \
        test-bin-converter.c

test_bin_converter_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
	libgda-test-6.0.la \
        $(COREDEPS_LIBS)

test_sql_identifier_SOURCES = \
        test-sql-identifier.c

test_sql_identifier_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_identifiers_quotes_SOURCES = \
        test-identifiers-quotes.c

test_identifiers_quotes_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_sql_builder_SOURCES = \
        test-sql-builder.c

test_sql_builder_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_connection_string_split_SOURCES = \
        test-connection-string-split.c

test_connection_string_split_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_input_parsers_SOURCES = \
        test-input-parsers.c

test_input_parsers_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_quark_list_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

test_sql_renderer_SOURCES = \
        test-sql-renderer.c

test_sql_renderer_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
	libgda-test-6.0.la \
        $(COREDEPS_LIBS)

test_server_operation_SOURCES = \
        test-server-operation.c

test_server_operation_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

EXTRA_DIST = dbstruct.xml
