providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-odbc.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	$(ODBC_CFLAGS)

libgda_odbc_la_SOURCES = \
	gda-odbc-provider.c \
	gda-odbc-provider.h \
	gda-odbc.h
libgda_odbc_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_odbc_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(ODBC_LIBS)
