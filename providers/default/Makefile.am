providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-default.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libgda_default_la_SOURCES = \
	gda-default-provider.c \
	gda-default-provider.h \
	gda-default-recordset.c \
	gda-default-recordset.h \
	gda-default.h \
	libmain.c
libgda_default_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_default_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la

