AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test-connection-4.0
noinst_PROGRAMS = gda-test-sql gda-test-models gda-test-server-operation gda-test-model-query gda-test-blob

gda_test_sql_SOURCES = \
	html.h \
	html.c \
        gda-test-sql.c

gda_test_sql_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_models_SOURCES = \
        html.h \
        html.c \
        gda-test-models.c

gda_test_models_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_connection_4_0_SOURCES = \
        gda-test-connection.c

gda_test_connection_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_server_operation_SOURCES = \
        gda-test-server-operation.c

gda_test_server_operation_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_model_query_SOURCES = \
	html.h \
	html.c \
        gda-test-model-query.c

gda_test_model_query_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_blob_SOURCES = \
        gda-test-blob.c

gda_test_blob_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)


EXTRA_DIST = 
