providerdir=$(libdir)/libgda/providers
provider_LTLIBRARIES = libgda-odbc.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ODBC_CFLAGS)

libgda_odbc_la_SOURCES = \
	gda-odbc-provider.c \
	gda-odbc-provider.h \
	gda-odbc-recordset.c \
	gda-odbc-recordset.h \
	utils.c \
	gda-odbc.h \
	libmain.c
libgda_odbc_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_odbc_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(ODBC_LIBS)
