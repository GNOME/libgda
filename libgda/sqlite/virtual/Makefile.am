if HAVE_SQLITE
sqlitelibs= $(SQLITE_LIBS)
sqliteinc= $(SQLITE_CFLAGS)
else
sqlitelibs= $(top_builddir)/libgda/sqlite/sqlite-src/libsqlite.la
sqliteinc= -I$(top_srcdir)/libgda/sqlite/sqlite-src \
	-I$(top_builddir)/libgda/sqlite/sqlite-src 
endif

noinst_LTLIBRARIES = libgda-virtual-6.0.la

if LDAP
GDA_LDAP_H=gda-ldap-connection.h
GDA_LDAP_S=gda-ldap-connection.c
DEF_FLAGS=-DHAVE_LDAP
endif


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(SQLITE_CFLAGS) \
	-I$(top_srcdir)/libgda/sqlite \
        -I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	$(sqliteinc) 

virtual_headers = \
	gda-vconnection-data-model.h \
	gda-vconnection-hub.h \
	gda-vprovider-data-model.h \
	gda-vprovider-hub.h \
	gda-virtual-connection.h \
	gda-virtual-provider.h \
	$(GDA_LDAP_H) \
	libgda-virtual.h

libgda_virtual_6_0_la_SOURCES = \
	$(virtual_headers) \
	gda-vconnection-data-model.c \
	gda-vconnection-data-model-private.h \
	gda-vconnection-hub.c \
	gda-vprovider-data-model.c \
	gda-vprovider-hub.c \
	gda-virtual-connection.c \
	$(GDA_LDAP_S) \
	gda-virtual-provider.c

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/virtual
gdainclude_HEADERS=$(virtual_headers)

xmldir   = $(datadir)/libgda-6.0
xml_in_files = 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files) 
DISTCLEANFILES = $(xml_DATA)

