providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-xml.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

libgda_xml_la_SOURCES = \
	gda-xml-provider.c \
	gda-xml-provider.h \
	gda-xml-recordset.c \
	gda-xml-recordset.h \
	gda-xml.h \
	libmain.c
libgda_xml_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_xml_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la

