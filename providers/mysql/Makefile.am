providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mysql.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MYSQL_CFLAGS) \
	-DGDA_PACKAGE_VERSION=\""$(PACKAGE_VERSION)"\"

libgda_mysql_la_SOURCES = \
	gda-mysql-ddl.c \
        gda-mysql-ddl.h \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql.h \
	libmain.c \
	utils.c
libgda_mysql_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_mysql_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(MYSQL_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = \
	mysql_specs_create_db.xml.in \
	mysql_specs_drop_db.xml.in \
	mysql_specs_dsn.xml.in \
	mysql_specs_create_table.xml.in \
        mysql_specs_drop_table.xml.in \
        mysql_specs_create_index.xml.in \
        mysql_specs_drop_index.xml.in \
	mysql_specs_rename_table.xml.in \
	mysql_specs_add_column.xml.in \
	mysql_specs_drop_column.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-mysql-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-mysql-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)
