providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-ldap.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(LDAP_CFLAGS)

libgda_ldap_la_SOURCES = \
	gdaprov-data-model-ldap.c \
	gdaprov-data-model-ldap.h \
	gda-ldap-provider.c \
	gda-ldap-provider.h \
	gda-ldap-util.c \
	gda-ldap-util.h \
	gda-ldap.h \
	libmain.c

libgda_ldap_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_ldap_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) \
	$(LDAP_LIBS)

xmldir   = $(datadir)/libgda-6.0
xml_in_files = ldap_specs_dsn.xml.in ldap_specs_auth.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-ldap-6.0.pc

EXTRA_DIST = $(xml_in_files) libgda-ldap-6.0.pc.in
DISTCLEANFILES = $(xml_DATA)

