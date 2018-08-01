QUIET_GEN = $(Q:@=@echo ' GEN '$@;)

lib_LTLIBRARIES = libgda-ui-6.0.la

if HAVE_GLADE
    GLADECATALOG = . glade
endif

SUBDIRS = marshallers data-entries internal data . data-entries/plugins demos $(GLADECATALOG)

GLOBAL_CFLAGS = \
	-I$(top_builddir)/libgda-ui \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-DPLUGINSDIR=\""$(top_builddir)/libgda-ui/plugins/.libs"\" \
	-DGDA_ABI_VERSION=\""$(GDA_ABI_VERSION)"\" \
	$(COREDEPS_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS)

AM_CPPFLAGS = \
	$(GLOBAL_CFLAGS) \
	$(COREDEPS_WFLAGS)

ui_headers = \
	gdaui-basic-form.h \
	gdaui-combo.h \
	gdaui-data-entry.h \
	gdaui-data-store.h \
	gdaui-data-filter.h \
	gdaui-data-selector.h \
	gdaui-data-proxy.h \
	gdaui-data-proxy-info.h \
	gdaui-decl.h \
	gdaui-easy.h \
	gdaui-enums.h \
	gdaui-form.h \
	gdaui-grid.h \
	gdaui-cloud.h \
	gdaui-login.h \
	gdaui-provider-selector.h \
	gdaui-raw-form.h \
	gdaui-raw-grid.h \
	gdaui-rt-editor.h \
	gdaui-server-operation.h \
	gdaui-tree-store.h \
	gdaui-plugin.h

ui_sources = \
	$(ui_headers) \
	gdaui-basic-form.c \
	gdaui-combo.c \
	gdaui-data-entry.c \
	gdaui-data-store.c \
	gdaui-data-filter.c \
	gdaui-data-selector.c \
	gdaui-data-proxy.c \
	gdaui-data-proxy-info.c \
	gdaui-form.c \
	gdaui-grid.c \
	gdaui-cloud.c \
	gdaui-login.c \
	gdaui-provider-selector.c \
	gdaui-raw-form.c \
	gdaui-raw-grid.c \
	bullet.h \
	bulleth.h \
	gdaui-rt-editor.c \
	gdaui-server-operation.c \
	gdaui-set.h \
	gdaui-set.c \
	gdaui-tree-store.c \
	gdaui-init.c

libgda_ui_6_0_la_SOURCES = \
	$(libgda_ui_built_cfiles) \
	$(ui_sources) \
	gdaui-resources.h \
	gdaui-resources.c

gdaui-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(ui_headers) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
	                --fhead "#ifndef __LIBGDAUI_ENUM_TYPES_H__\n#define __LIBGDAUI_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDAUI_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBGDAUI_ENUM_TYPES_H__ */" \
		$(ui_headers) ) > tmp-gdaui-enum-types.h \
	&& (cmp -s tmp-gdaui-enum-types.h gdaui-enum-types.h || cp tmp-gdaui-enum-types.h gdaui-enum-types.h ) \
	&& rm -f tmp-gdaui-enum-types.h	\
	&& echo timestamp > $(@F)

gdaui-enum-types.c: s-enum-types-c
	@true

s-enum-types-c: @REBUILD@ $(ui_headers) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
	                --fhead "#include <libgda-ui/libgda-ui.h>\n" \
	                --fhead "#include <glib-object.h>" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(ui_headers) ) > tmp-gdaui-enum-types.c \
	&& (cmp -s tmp-gdaui-enum-types.c gdaui-enum-types.c || cp tmp-gdaui-enum-types.c gdaui-enum-types.c ) \
	&& rm -f tmp-gdaui-enum-types.c \
	&& echo timestamp > $(@F)


# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
libgda_ui_built_headers = gdaui-enum-types.h
libgda_ui_built_cfiles = gdaui-enum-types.c

$(OBJECTS) $(libgda_ui_6_0_la_OBJECTS): gdaui-resources.h gdaui-resources.c $(libgda_ui_built_headers) $(libgda_ui_built_cfiles)

libgda_ui_6_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	$(NO_UNDEFINED) $(LIBTOOL_UI_EXPORT_OPTIONS)
libgda_ui_6_0_la_LIBADD = \
	marshallers/libgda-ui-marshallers.la \
	internal/libgda-ui-internal.la \
	data-entries/libgda-ui-data-entries.la \
	../libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) \
	$(GTK_LIBS) \
	$(GIO_LIBS)

if PLATFORM_WIN32
libgda_ui_6_0_la_LDFLAGS += -export-symbols $(builddir)/libgda-ui.def
libgda_ui_6_0_la_DEPENDENCIES = libgda-ui.def

libgda-ui.def: libgda-ui.symbols
	(echo -e EXPORTS; $(CPP) -P - <$(srcdir)/libgda-ui.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgda-ui.def.tmp && mv libgda-ui.def.tmp libgda-ui.def

libgda-ui-$(GDA_ABI_VERSION).lib: libgda-ui.def
	$(DLLTOOL) --dllname libgda-ui-$(GDA_ABI_VERSION)-$(GDA_ABI_MAJOR_VERSION).dll --def libgda-ui.def --output-lib libgda-ui-$(GDA_ABI_VERSION).lib

install-data-local: install-def-files
uninstall-local: uninstall-def-files

install-def-files: libgda-ui.def libgda-ui-$(GDA_ABI_VERSION).lib
	$(INSTALL) $(builddir)/libgda-ui.def $(DESTDIR)$(libdir)/libgda-ui-$(GDA_ABI_VERSION).def
	$(INSTALL) $(builddir)/libgda-ui-$(GDA_ABI_VERSION).lib $(DESTDIR)$(libdir)/

uninstall-def-files:
	-rm $(DESTDIR)$(libdir)/libgda-ui-$(GDA_ABI_VERSION).def
	-rm $(DESTDIR)$(libdir)/libgda-ui-$(GDA_ABI_VERSION).lib
endif

gdauiincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-ui
gdauiinclude_HEADERS= libgda-ui.h $(ui_headers) $(libgda_ui_built_headers)

CLEANFILES = $(libgda_ui_built_headers) $(libgda_ui_built_cfiles) \
	gdaui-enum-types.h \
	s-enum-types-h \
	gdaui-enum-types.c \
	s-enum-types-c \
	libgda-ui.def \
	libgda-ui-$(GDA_ABI_VERSION).lib \
	gdaui-resources.h \
	gdaui-resources.c

DTD_FILES = gdaui-layout.dtd
dtddir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/dtd
dtd_DATA=$(DTD_FILES)

gdaui-resources.h gdaui-resources.c: $(srcdir)/gdaui.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --sourcedir=$(builddir) --generate-dependencies $(srcdir)/gdaui.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(builddir) --generate --c-name _gdaui --manual-register $<

EXTRA_DIST = $(DTD_FILES) \
	libgda-ui.symbols \
	gdaui.css \
	gdaui.gresource.xml

if ENABLE_GDAUI_GI
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) \
                             --add-include-path=$(top_builddir)/libgda \
                             --add-include-path=$(top_builddir)/libgda-ui \
                             --namespace Gdaui \
                             --warn-all \
                             --c-include=libgda-ui/libgda-ui.h
ui_data_entries_sources = \
	data-entries/gdaui-data-cell-renderer-bin.h \
	data-entries/gdaui-data-cell-renderer-boolean.h \
	data-entries/gdaui-data-cell-renderer-combo.h \
	data-entries/gdaui-data-cell-renderer-info.h \
	data-entries/gdaui-data-cell-renderer-textual.h \
	data-entries/gdaui-entry-bin.h \
	data-entries/gdaui-entry-boolean.h \
	data-entries/gdaui-entry-combo.h \
	data-entries/gdaui-entry-none.h \
	data-entries/gdaui-entry-shell.h \
	data-entries/gdaui-entry-string.h \
	data-entries/gdaui-entry-number.h \
	data-entries/gdaui-entry-common-time.h \
	data-entries/gdaui-entry-time.h \
	data-entries/gdaui-entry-timestamp.h \
	data-entries/gdaui-entry-date.h \
	data-entries/gdaui-entry-wrapper.h \
	data-entries/gdaui-entry.h \
	data-entries/gdaui-formatted-entry.h \
	data-entries/gdaui-numeric-entry.h \
	data-entries/gdaui-data-cell-renderer-bin.c \
	data-entries/gdaui-data-cell-renderer-boolean.c \
	data-entries/gdaui-data-cell-renderer-combo.c \
	data-entries/gdaui-data-cell-renderer-info.c \
	data-entries/gdaui-data-cell-renderer-textual.c \
	data-entries/gdaui-entry-bin.c \
	data-entries/gdaui-entry-boolean.c \
	data-entries/gdaui-entry-combo.c \
	data-entries/gdaui-entry-none.c \
	data-entries/gdaui-entry-shell.c \
	data-entries/gdaui-entry-string.c \
	data-entries/gdaui-entry-number.c \
	data-entries/gdaui-entry-common-time.c \
	data-entries/gdaui-entry-time.c \
	data-entries/gdaui-entry-timestamp.c \
	data-entries/gdaui-entry-date.c \
	data-entries/gdaui-entry-wrapper.c \
	data-entries/gdaui-entry.c \
	data-entries/gdaui-formatted-entry.c \
	data-entries/gdaui-numeric-entry.c

introspection_sources = \
	$(ui_headers) \
	$(ui_sources) \
	$(ui_data_entries_sources)

Gdaui-6.0.gir: $(lib_LTLIBRARIES)
Gdaui_6_0_gir_INCLUDES = Gtk-3.0 Gda-6.0
Gdaui_6_0_gir_CFLAGS = $(GLOBAL_CFLAGS)
Gdaui_6_0_gir_LIBS = $(lib_LTLIBRARIES) ../libgda/libgda-6.0.la
Gdaui_6_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libgda
INTROSPECTION_GIRS += Gdaui-6.0.gir

if ENABLE_GI_SYSTEM_INSTALL
girdir = $(INTROSPECTION_GIRDIR)
typelibsdir = $(INTROSPECTION_TYPELIBDIR)
else
girdir = $(datadir)/gir-1.0
typelibsdir = $(libdir)/girepository-1.0
endif

gir_DATA = $(INTROSPECTION_GIRS)
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(typelibs_DATA) $(gir_DATA)

if ENABLE_GDAUI_VALA

vapi: libgdaui-6.0.vapi

libgdaui-6.0.vapi: Gdaui-6.0.gir
	$(VAPIGEN) \
		--metadatadir=$(top_srcdir)/libgda-ui \
		--vapidir=$(top_builddir)/libgda \
		--girdir=$(top_builddir)/libgda \
		--library=libgdaui-6.0 \
		--pkg=libxml-2.0 \
		--pkg=libgda-6.0 \
		--pkg=gio-2.0 \
		--pkg=atk \
		--pkg=gtk+-3.0 \
		Gdaui-6.0.gir

vapidir = $(VAPIDIR)
vapi_DATA=libgdaui-6.0.vapi

EXTRA_DIST += \
	Gdaui-6.0.metadata \
        $(vapi_DATA)

DISTCLEANFILES=libgdaui-6.0.vapi

endif

endif
