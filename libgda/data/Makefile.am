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
    $(COREDEPS_CFLAGS) \
    $(LIBGDA_WFLAGDS) \
    $(LIBGDA_CFLAGS) \
    $(GEE_CFLAGS) \
    $(VALA_CFLAGS) \
    $(NULL)

lib_LTLIBRARIES = libgdadata-1.0.la

VALAFLAGS = \
    --vapidir=$(top_srcdir)/libgda \
    --pkg libxml-2.0 \
    --pkg gee-1.0 \
    --pkg libgda-5.0 \
    $(NULL)

libgdadata_1_0_la_VALASOURCES = \
	DataObject.vala \
	SelectQuery.vala \
	$(NULL)

libgdadata_1_0_la_SOURCES = \
	gdadata.vala.stamp \
	$(libgdadata_1_0_la_VALASOURCES:.vala=.c) \
	$(NULL)	

gdadata-1.0.vapi gdadata.vala.stamp GdaData-1.0.gir: $(libgdadata_1_0_la_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H libgdadata.h --gir=GdaData-1.0.gir --library gdadata-1.0 $^
	@touch $@

CLEANFILES+= gdadata-1.0.vapi GdaData-1.0.gir

libgdadata_1_0_la_LDFLAGS = 

libgdadata_1_0_la_LIBADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(LIBGDA_LIBS) \
	$(GEE_LIBS) \
	$(VALA_LIBS) \
	$(NULL)

include_HEADERS = \
	libgdadata.h \
	$(NULL)

gdadataincludedir = $(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/data
gdadatainclude_HEADERS = libgdadata.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgdadata-1.0.pc

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
INTROSPECTION_GIRS += GdaData-1.0.gir
INTROSPECTION_COMPILER_ARGS = 

GdaData-1.0.typelib: $(INTROSPECTION_GIRS)
	$(INTROSPECTION_COMPILER) $(INTROSPECTION_COMPILER_ARGS) --includedir=. $< -o $@

gir_DATA = $(INTROSPECTION_GIRS)
typelibs_DATA = GdaData-1.0.typelib

vapidir = $(VALA_VAPIDIR)
vapi_DATA = gdadata-1.0.vapi

CLEANFILES += $(INTROSPECTION_GIRS) $(typelibs_DATA) gdadata-1.0.vapi $(libgdadata_1_0_la_VALASOURCES:.vala=.c)

EXTRA_DIST = \
	libgdadata-1.0.pc.in \
	$(libgdadata_1_0_la_VALASOURCES:.vala=.c) \
	$(vapi_DATA) \
	$(INTROSPECTION_GIRS) \
	$(typelibs_DATA) \
	gdadata.vala.stamp

