providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-oracle.la

configdir   = $(datadir)/libgda
config_DATA = oracle_specs_dsn.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_oracle_la_SOURCES = \
	gda-oracle-provider.c \
	gda-oracle-provider.h \
	gda-oracle-recordset.c \
	gda-oracle-recordset.h \
	gda-oracle.h \
	libmain.c \
	utils.c
libgda_oracle_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_oracle_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la \
	$(ORACLE_LIBS)

EXTRA_DIST = $(config_DATA)

