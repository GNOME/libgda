NULL =

if ENABLE_VALA_EXTENSIONS

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

lib_LTLIBRARIES = libgdadata.la

VALAFLAGS = \
    $(top_srcdir)/libgda/data/config.vapi \
    --vapidir=$(top_srcdir)/libgda \
    --pkg libxml-2.0 \
    --pkg gee-1.0 \
    --pkg libgda-5.0 \
    $(NULL)

libgdadata_la_VALASOURCES = \
	DataObject.vala \
	SelectQuery.vala \
	$(NULL)

libgdavala_la_SOURCES = \
	gdadata.vala.stamp \
	$(libgdadata_la_VALASOURCES:.vala=.c) \
	$(NULL)	

gdadata-1.0.vapi gdadata.vala.stamp GdaData-1.0.gir: $(libgdadata_la_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H libgdadata.h --gir=GdaData-1.0.gir --library gdadata-1.0 $^
	@touch $@

CLEANFILES+= gdadata-1.0.vapi GdaData-1.0.gir

libgdavala_la_LDFLAGS = 

libgdavala_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(GEE_LIBS) \
	$(VALA_LIBS) \
	$(NULL)

include_HEADERS = \
	libgdadata.h \
	$(NULL)

gdavalaincludedir = $(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/data
gdavalainclude_HEADERS = libgdadata.h

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

CLEANFILES += $(INTROSPECTION_GIRS) $(typelibs_DATA) gdadata-1.0.vapi 

EXTRA_DIST = \
	libgdadata-1.0.pc.in \
	$(libgdadata_la_VALASOURCES) \
	$(vapi_DATA) \
	$(INTROSPECTION_GIRS) \
	$(typelibs_DATA) \
	gdadata.vala.stamp

endif
