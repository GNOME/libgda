providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-msql.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MSQL_CFLAGS)

libgda_msql_la_SOURCES = \
	gda-msql-provider.c \
	gda-msql-provider.h \
	gda-msql-recordset.c \
	gda-msql-recordset.h \
	gda-msql.h \
	gda-msql-libmain.c \
	gda-msql-utils.c
libgda_msql_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_msql_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la \
	$(MSQL_LIBS)
