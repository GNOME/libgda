AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test-connection-4.0
noinst_PROGRAMS = gda-test-server-operation gda-test-blob

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

gda_test_blob_SOURCES = \
        gda-test-blob.c

gda_test_blob_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)


EXTRA_DIST = 
