providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-xbase.la

AM_CPPFLAGS = \
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

libgda_xbase_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_xbase_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(XBASE_LIBS)

