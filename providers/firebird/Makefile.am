providerdir=$(libdir)/libgda/providers
provider_LTLIBRARIES = libgda-firebird.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(FIREBIRD_CFLAGS)

libgda_firebird_la_SOURCES = \
	gda-firebird-provider.c \
	gda-firebird-provider.h \
	gda-firebird-recordset.c \
	gda-firebird-recordset.h \
	libmain.c

libgda_firebird_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_firebird_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(FIREBIRD_LIBS)

