providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-interbase.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(INTERBASE_CFLAGS)

libgda_interbase_la_SOURCES = \
	gda-interbase-provider.c \
	gda-interbase-provider.h \
	gda-interbase-recordset.c \
	gda-interbase-recordset.h \
	libmain.c

libgda_interbase_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_interbase_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(INTERBASE_LIBS)

