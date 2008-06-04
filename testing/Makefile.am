AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test-connection-4.0
noinst_PROGRAMS = gda-test-blob gda-provider-status

gda_test_connection_4_0_SOURCES = \
        gda-test-connection.c

gda_test_connection_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_test_blob_SOURCES = \
        gda-test-blob.c

gda_test_blob_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)


gda_provider_status_SOURCES = \
	html.h \
	html.c \
        gda-provider-status.c

gda_provider_status_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = 
