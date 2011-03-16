providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mysql.la

#Rem: MYSQL_CFLAGS and MYSQL_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I$(top_srcdir)/providers/reuseable/mysql \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) $(MYSQL_CFLAGS) 

libgda_mysql_la_SOURCES = \
	gda-mysql-blob-op.c \
	gda-mysql-blob-op.h \
	gda-mysql-ddl.c \
	gda-mysql-ddl.h \
	gda-mysql-handler-boolean.c \
	gda-mysql-handler-boolean.h \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql-util.c \
	gda-mysql-util.h \
	gda-mysql-pstmt.c \
	gda-mysql-pstmt.h \
	gda-mysql.h \
	libmain.c

libgda_mysql_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_mysql_la_LIBADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/providers/reuseable/mysql/libgda-mysql.la \
	$(COREDEPS_LIBS) $(MYSQL_LIBS)

xmldir   = $(datadir)/libgda-5.0
xml_in_files = \
	mysql_specs_create_db.xml.in \
	mysql_specs_drop_db.xml.in \
	mysql_specs_dsn.xml.in \
	mysql_specs_create_table.xml.in \
        mysql_specs_drop_table.xml.in \
        mysql_specs_comment_table.xml.in \
        mysql_specs_create_index.xml.in \
        mysql_specs_drop_index.xml.in \
	mysql_specs_rename_table.xml.in \
	mysql_specs_add_column.xml.in \
	mysql_specs_drop_column.xml.in \
	mysql_specs_comment_column.xml.in \
	mysql_specs_create_view.xml.in \
	mysql_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-mysql-5.0.pc

EXTRA_DIST = $(xml_in_files) libgda-mysql-5.0.pc.in
DISTCLEANFILES = $(xml_DATA)
