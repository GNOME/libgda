providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-jdbc.la
noinst_PROGRAMS = gda-jdbc-test
bin_PROGRAMS = gda-list-jdbc-providers-5.0

#Rem: JDBC_CFLAGS and JDBC_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) $(JAVA_CFLAGS) \
	-DJVM_PATH=\""$(JAVA_LD_PATH)"\" \
	-DTOP_BUILD_DIR=\""$(top_builddir)"\"

jni_sources = \
	GdaJConnection.c \
	GdaJMeta.c \
	GdaJProvider.c \
	GdaJPStmt.c \
	GdaJResultSet.c \
	GdaJResultSetInfos.c \
	GdaJColumnInfos.c \
	GdaJBlobOp.c \
	GdaJValue.c \
	GdaInputStream.c

jni_headers = $(jni_sources:.c=.h) 
jni_classes = $(jni_sources:.c=.class) 

$(jni_sources): $(jni_headers)
path_jni_sources = $(addprefix $(srcdir)/,$(jni_sources))

libgda_jdbc_la_SOURCES = \
	gda-jdbc-blob-op.c \
	gda-jdbc-blob-op.h \
	gda-jdbc-ddl.c \
	gda-jdbc-ddl.h \
	gda-jdbc-provider.c \
	gda-jdbc-provider.h \
	gda-jdbc-pstmt.h \
	gda-jdbc-pstmt.c \
	gda-jdbc-meta.c \
	gda-jdbc-meta.h \
	gda-jdbc-recordset.c \
	gda-jdbc-recordset.h \
	gda-jdbc-util.c \
	gda-jdbc-util.h \
	gda-jdbc.h \
	libmain.c \
	jni-globals.h \
	jni-wrapper.h \
	jni-wrapper.c \
	$(path_jni_sources) \
	$(jni_headers)


libgda_jdbc_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_jdbc_la_LIBADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)
# rem: don't link with JAVA_LIBS because we don't want the dependency on it
# (JVM runtime will be loaded when running)

# rem: if on Windows, then we need to add these flags because we don't want the exported symbols's names
# to be mangled with @8 for example
if PLATFORM_WIN32
libgda_jdbc_la_LDFLAGS += -Wl,--kill-at
endif


$(jni_headers): $(jdbcprov_classes)
	$(JAVAH) -classpath . -force `basename $@ | sed -e 's/\.h$$//'`

jdbcprov_jar = gdaprovider-5.0.jar
jdbcprov_sources = \
	provider.java \
	meta.java \
	derby.java \
	h2.java
jdbcprov_classes = GdaJConnection.class

jdbcprovdir = $(providerdir)
jdbcprov_DATA = $(jdbcprov_jar)

GdaJConnection.class: $(jdbcprov_sources)
	$(JAVAC) -cp . $(JFLAGS) -d $(top_builddir)/providers/jdbc $^

$(jdbcprov_jar): $(jdbcprov_classes)
	$(JAR) cfm $(jdbcprov_jar) $(top_srcdir)/providers/jdbc/MANIFEST.MF *.class

clean-local:
	rm -rf $(jdbcprov_jar)
	rm -f $(jni_headers)
	rm -f *.class

xmldir   = $(datadir)/libgda-5.0
xml_in_files = \
	jdbc_specs_dsn.xml.in \
	jdbc_specs_create_table.xml.in 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-jdbc-5.0.pc

doc_files = \
	doc/structure.dia

EXTRA_DIST = $(xml_in_files) libgda-jdbc-5.0.pc.in \
	$(jdbcprov_sources) \
	$(doc_files) \
	MANIFEST.MF
DISTCLEANFILES = $(xml_DATA)



gda_jdbc_test_SOURCES = gda-jdbc-test.c
gda_jdbc_test_LDADD = \
        $(COREDEPS_LIBS) \
        $(top_builddir)/libgda/libgda-5.0.la


gda_list_jdbc_providers_5_0_SOURCES = gda-list-jdbc-providers.c
gda_list_jdbc_providers_5_0_LDADD = \
        $(COREDEPS_LIBS) \
        $(top_builddir)/libgda/libgda-5.0.la

