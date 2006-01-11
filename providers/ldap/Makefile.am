providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-ldap.la

configdir   = $(datadir)/libgda
config_DATA = ldap_specs_dsn.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(LDAP_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_ldap_la_SOURCES = \
	gda-ldap-provider.c \
	gda-ldap-provider.h \
	gda-ldap.h \
	gda-ldap-recordset.c \
	gda-ldap-recordset.h \
	libmain.c \
	utils.c
libgda_ldap_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_ldap_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(LDAP_LIBS)

EXTRA_DIST = $(config_DATA)
