providerdir=$(libdir)/libgda/providers
provider_LTLIBRARIES = libgda-ldap.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
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
libgda_ldap_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_ldap_la_LIBADD = \
	$(top_builddir)/libgda/libgda-2.la \
	$(LIBGDA_LIBS) \
	$(LDAP_LIBS)

