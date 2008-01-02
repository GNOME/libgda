providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-ldap.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(LDAP_CFLAGS)

libgda_ldap_la_SOURCES = \
	gda-ldap-provider.c \
	gda-ldap-provider.h \
	gda-ldap.h \
	gda-ldap-recordset.c \
	gda-ldap-recordset.h \
	libmain.c \
	utils.c
libgda_ldap_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_ldap_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(LDAP_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = ldap_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-ldap-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-ldap-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)

