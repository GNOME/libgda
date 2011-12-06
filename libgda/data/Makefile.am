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

lib_LTLIBRARIES = libgdadata-0.2.la

VALAFLAGS = \
    --vapidir=$(top_srcdir)/libgda \
    --pkg libxml-2.0 \
    --pkg gee-0.8 \
    --pkg libgda-5.0 \
    $(NULL)

libgdadata_0_2_la_VALASOURCES = \
	DataObject.vala \
	SelectQuery.vala \
	DataModelIterator.vala \
	$(NULL)

libgdadata_0_2_la_SOURCES = \
	gdadata.vala.stamp \
	$(libgdadata_0_2_la_VALASOURCES:.vala=.c) \
	$(NULL)	

gdadata-0.2.vapi gdadata.vala.stamp GdaData-0.2.gir: $(libgdadata_0_2_la_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H libgdadata.h --gir=GdaData-0.2.gir --library gdadata-0.2 $^
	@touch $@

CLEANFILES+= gdadata-0.2.vapi GdaData-0.2.gir

libgdadata_0_2_la_LDFLAGS = 

libgdadata_0_2_la_LIBADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(LIBGDA_LIBS) \
	$(GEE_LIBS) \
	$(VALA_LIBS) \
	$(NULL)

include_HEADERS = \
	libgdadata.h \
	$(NULL)

gdadataincludedir = $(includedir)/libgdadata-0.2/
gdadatainclude_HEADERS = libgdadata.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgdadata-0.2.pc

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
INTROSPECTION_GIRS += GdaData-0.2.gir
INTROSPECTION_COMPILER_ARGS = 

GdaData-0.2.typelib: $(INTROSPECTION_GIRS)
	$(INTROSPECTION_COMPILER) $(INTROSPECTION_COMPILER_ARGS) --includedir=. $< -o $@

gir_DATA = $(INTROSPECTION_GIRS)
typelibs_DATA = GdaData-0.2.typelib

vapidir = $(VALA_VAPIDIR)
vapi_DATA = gdadata-0.2.vapi

CLEANFILES += \
    $(INTROSPECTION_GIRS) \
    $(typelibs_DATA) \
    gdadata-0.2.vapi \
    libgdadata-0.2.pc \
    libgdadata.h \
    $(libgdadata_0_2_la_VALASOURCES:.vala=.c) \
    $(NULL)

EXTRA_DIST = \
	libgdadata-0.2.pc \
	$(libgdadata_0_2_la_VALASOURCES:.vala=.c) \
	$(vapi_DATA) \
	$(INTROSPECTION_GIRS) \
	$(typelibs_DATA) \
	gdadata.vala.stamp

