providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-sybase.la

INCLUDES = \
	$(SYBASE_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

libgda_sybase_la_SOURCES = \
	gda-sybase-provider.c \
	gda-sybase-provider.h \
	gda-sybase.h \
	gda-tds-schemas.h \
	libmain.c \
	utils.c

libgda_sybase_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_sybase_la_LIBADD = \
	$(SYBASE_LIBS) \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la 

