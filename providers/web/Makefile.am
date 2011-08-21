providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-web.la

#Rem: WEB_CFLAGS and WEB_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) $(LIBSOUP_CFLAGS) 

libgda_web_la_SOURCES = \
	gda-web-blob-op.c \
	gda-web-blob-op.h \
	gda-web-ddl.c \
	gda-web-ddl.h \
	gda-web-provider.c \
	gda-web-provider.h \
	gda-web-pstmt.h \
	gda-web-pstmt.c \
	gda-web-meta.c \
	gda-web-meta.h \
	gda-web-recordset.c \
	gda-web-recordset.h \
	gda-web-util.h \
	gda-web-util.c \
	gda-web.h \
	$(top_srcdir)/libgda/global.h \
	$(top_srcdir)/libgda/md5.h \
	$(top_srcdir)/libgda/md5c.c \
	libmain.c

libgda_web_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_web_la_LIBADD = \
	$(top_builddir)/providers/reuseable/libreuseable.la \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS) $(LIBSOUP_LIBS)

xmldir   = $(datadir)/libgda-5.0
xml_in_files = \
	web_specs_dsn.xml.in \
	web_specs_auth.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-web-5.0.pc

phpdir = $(datadir)/libgda-5.0/php
php_DATA = \
	php/README \
	php/gda-clean.php \
	php/gda-config.php \
	php/gda-secure-config.php \
	php/gda-exception.php \
	php/gda-front.php \
	php/gda-meta.php \
	php/gda-setup.php \
	php/gda-tester.php \
	php/gda-utils.php \
	php/gda-worker.php


EXTRA_DIST = $(xml_in_files) libgda-web-5.0.pc.in \
	$(php_DATA)

DISTCLEANFILES = $(xml_DATA)
