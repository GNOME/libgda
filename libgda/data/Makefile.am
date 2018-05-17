AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

AM_CFLAGS =\
	-Wall \
	-g \
	-I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        -I$(top_builddir)/libgda/data \
        $(COREDEPS_CFLAGS) \
        $(LIBGDA_WFLAGDS) \
        $(LIBGDA_CFLAGS) \
        $(GEE_CFLAGS) \
        $(VALA_CFLAGS)

lib_LTLIBRARIES = libgdadata-6.0.la

AM_VALAFLAGS = \
    --vapidir=$(top_srcdir)/libgda \
    --vapidir=$(VAPIDIR) \
    --includedir=$(top_srcdir)/libgda \
    --pkg libxml-2.0 \
    --pkg gee-0.8 \
    --pkg libgda-6.0 \
    -C -H $(include_headers) \
    --gir=GdaData-6.0.gir \
    --library gdadata-6.0

object_persistance_sources = \
	DataBase.vala \
	Catalog.vala \
	Schema.vala \
	Table.vala \
	Record.vala \
	SelectQuery.vala

db_collection_sources = \
	DbObject.vala \
	DbCollection.vala \
	DbCatalog.vala \
	DbTable.vala \
	DbRecord.vala \
	DbField.vala \
	DbFieldInfo.vala \
	DbSchema.vala \
	DbRecordCollection.vala \
	RecordCollection.vala \
	RecordCollectionIterator.vala \
	Field.vala \
	FieldInfo.vala

include_headers = libgdadata.h

libgdadata_6_0_la_SOURCES = \
	NamespaceInfo.vala \
	$(object_persistance_sources) \
	$(db_collection_sources)

$(include_headers): DataBase.c
vapi: gdadata-6.0.vapi

vapidir = $(VAPIDIR)
vapi_DATA = gdadata-6.0.vapi gdadata-6.0.deps

gdadata-6.0.vapi GdaData-6.0.gir: libgdadata-6.0.la

CLEANFILES= gdadata-6.0.vapi

libgdadata_6_0_la_LDFLAGS = 

libgdadata_6_0_la_LIBADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
	$(LIBGDA_LIBS) \
	$(GEE_LIBS) \
	$(VALA_LIBS)

gdadataincludedir = $(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/data
gdadatainclude_HEADERS = $(include_headers)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgdadata-6.0.pc

# GObject Introspection

if ENABLE_GI_SYSTEM_INSTALL
girdir = $(INTROSPECTION_GIRDIR)
typelibsdir = $(INTROSPECTION_TYPELIBDIR)
else
girdir = $(datadir)/gir-1.0
typelibsdir = $(libdir)/girepository-1.0
endif

# GIR files are generated automatically by Valac then is not necessary to scan source code to generate it
INTROSPECTION_GIRS = GdaData-6.0.gir

# Extract dlname from libfolks.la; see bug #658002.
# This is what g-ir-scanner does.
libgdadata_dlname = \
	`$(SED) -n "s/^dlname='\([A-Za-z0-9.+-]\+\)'/\1/p" libgdadata-6.0.la`
INTROSPECTION_COMPILER_ARGS = \
    --includedir=. \
    --includedir=$(top_srcdir)/libgda \
    --includedir=$(top_srcdir)/libgda/data \
    --includedir=$(top_builddir)/libgda \
    --includedir=$(top_builddir)/libgda/data \
    -l $(libgdadata_dlname)

GdaData-6.0.typelib: $(INTROSPECTION_GIRS)
	$(INTROSPECTION_COMPILER) $(INTROSPECTION_COMPILER_ARGS)  $< -o $@

gir_DATA = $(INTROSPECTION_GIRS)
typelibs_DATA = GdaData-6.0.typelib

CLEANFILES += \
        $(typelibs_DATA) \
        gdadata-6.0.vapi \
        libgdadata-6.0.pc \
        $(include_headers) \
        $(libgdadata_6_0_la_VALASOURCES:.vala=.c) \
        $(srcdir)/libgdadata_6_0_la_vala.stamp

EXTRA_DIST = \
	libgdadata-6.0.pc \
	$(vapi_DATA) \
	$(INTROSPECTION_GIRS) \
	$(typelibs_DATA)

