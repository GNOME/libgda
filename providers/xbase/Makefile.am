providerdir=$(libdir)/libgda/providers
provider_LTLIBRARIES = libgda-xbase.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(XBASE_CFLAGS)

libgda_xbase_la_SOURCES = \
	gda-xbase-database.cpp \
	gda-xbase-database.h \
	gda-xbase-provider.c \
	gda-xbase-provider.h \
	libmain.c

libgda_xbase_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_xbase_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(XBASE_LIBS)

