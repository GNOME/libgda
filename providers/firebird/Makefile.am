providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-firebird.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(FIREBIRD_CFLAGS)

libgda_firebird_la_SOURCES = \
	gda-firebird-provider.c \
	gda-firebird-provider.h \
	gda-firebird-recordset.c \
	gda-firebird-recordset.h \
	gda-firebird-blob.c \
	gda-firebird-blob.h \
	libmain.c

libgda_firebird_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_firebird_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la \
	$(FIREBIRD_LIBS)

