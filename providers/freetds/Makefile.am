providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-freetds.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(FREETDS_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_freetds_la_SOURCES = \
	gda-freetds-defs.h \
	gda-freetds-provider.c \
	gda-freetds-provider.h \
	gda-freetds-recordset.c \
	gda-freetds-recordset.h \
	gda-freetds-types.c \
	gda-freetds-types.h \
	gda-freetds-message.c \
	gda-freetds-message.h \
	gda-freetds.h \
	gda-tds-schemas.h \
	libmain.c \
	utils.c

libgda_freetds_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_freetds_la_LIBADD = \
	$(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(FREETDS_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = freetds_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-freetds-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-freetds-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)

