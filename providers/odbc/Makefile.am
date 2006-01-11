providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-odbc.la

configdir   = $(datadir)/libgda
config_DATA = odbc_specs_dsn.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ODBC_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_odbc_la_SOURCES = \
	gda-odbc-provider.c \
	gda-odbc-provider.h \
	gda-odbc-recordset.c \
	gda-odbc-recordset.h \
	utils.c \
	gda-odbc.h \
	libmain.c
libgda_odbc_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_odbc_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(ODBC_LIBS)

EXTRA_DIST = $(config_DATA)
