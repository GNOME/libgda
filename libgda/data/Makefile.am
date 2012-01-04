NULL =

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

BUILT_SOURCES = gdadata.vala.stamp
CLEANFILES = gdadata.vala.stamp

AM_CFLAGS =\
	 -Wall\
	 -g \
	 -I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
    -I$(top_builddir)/libgda/data \
    $(COREDEPS_CFLAGS) \
    $(LIBGDA_WFLAGDS) \
    $(LIBGDA_CFLAGS) \
    $(GEE_CFLAGS) \
    $(VALA_CFLAGS) \
    $(NULL)

lib_LTLIBRARIES = libgdadata-5.0.la

VALAFLAGS = \
    --vapidir=$(top_srcdir)/libgda \
    --includedir=$(top_srcdir)/libgda \
    --pkg libxml-2.0 \
    --pkg gee-0.8 \
    --pkg libgda-5.0 \
    $(NULL)

object_persistance_sources = \
	DataObject.vala \
	DataObjectSingleId.vala \
	SelectQuery.vala \
	$(NULL)

db_collection_sources = \
	DbObject.vala \
	DbCollection.vala \
	DbTable.vala \
	DbRecord.vala \
	DbField.vala \
	DbSchema.vala \
	Field.vala \
	$(NULL)

selectable_sources = \
	SqlWhere.vala \
	SqlExpression.vala \
	$(NULL)

data_model_collection_sources = \
	DataModelIterator.vala \
	DataModelIterable.vala \
	$(NULL)

libgdadata_5_0_la_VALASOURCES = \
	NamespaceInfo.vala \
	$(object_persistance_sources) \
	$(db_collection_sources) \
	$(data_model_collection_sources) \
	$(NULL)

libgdadata_5_0_la_SOURCES = \
	gdadata.vala.stamp \
	$(libgdadata_5_0_la_VALASOURCES:.vala=.c) \
	$(NULL)	

vapi: gdadata-5.0.vapi

vapidir = $(VAPIDIR)
vapi_DATA = gdadata-5.0.vapi

gdadata-5.0.vapi gdadata.vala.stamp GdaData-5.0.gir: $(libgdadata_5_0_la_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H libgdadata.h --gir=GdaData-5.0.gir --library gdadata-5.0 $^
	@touch $@

CLEANFILES+= gdadata-5.0.vapi

libgdadata_5_0_la_LDFLAGS = 

libgdadata_5_0_la_LIBADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(LIBGDA_LIBS) \
	$(GEE_LIBS) \
	$(VALA_LIBS) \
	$(NULL)

include_HEADERS = \
	libgdadata.h \
	$(NULL)

gdadataincludedir = $(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda
gdadatainclude_HEADERS = libgdadata.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgdadata-5.0.pc

# GObject Introspection

if ENABLE_GI_SYSTEM_INSTALL
girdir = $(INTROSPECTION_GIRDIR)
typelibsdir = $(INTROSPECTION_TYPELIBDIR)
else
girdir = $(datadir)/gir-1.0
typelibsdir = $(libdir)/girepository-1.0
endif

# GIR files are generated automatically by Valac then is not necessary to scan source code to generate it
INTROSPECTION_GIRS =
INTROSPECTION_GIRS += GdaData-5.0.gir
INTROSPECTION_COMPILER_ARGS = \
    --includedir=. \
    --includedir=$(top_srcdir)/libgda \
    --includedir=$(top_srcdir)/libgda/data \
    $(NULL)

GdaData-5.0.typelib: $(INTROSPECTION_GIRS)
	$(INTROSPECTION_COMPILER) $(INTROSPECTION_COMPILER_ARGS)  $< -o $@

gir_DATA = $(INTROSPECTION_GIRS)
typelibs_DATA = GdaData-5.0.typelib

CLEANFILES += \
    $(typelibs_DATA) \
    gdadata-5.0.vapi \
    libgdadata-5.0.pc \
    libgdadata.h \
    $(libgdadata_5_0_la_VALASOURCES:.vala=.c) \
    $(NULL)

EXTRA_DIST = \
	libgdadata-5.0.pc \
	$(libgdadata_5_0_la_VALASOURCES:.vala=.c) \
	$(vapi_DATA) \
	$(INTROSPECTION_GIRS) \
	$(typelibs_DATA) \
	gdadata.vala.stamp

