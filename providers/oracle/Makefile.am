providerdir=$(libdir)/libgda/providers
provider_LTLIBRARIES = libgda-oracle.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS)

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
	$(top_builddir)/libgda/libgda-2.la \
	$(ORACLE_LIBS)
