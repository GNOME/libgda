QUIET_GEN = $(Q:@=@echo ' GEN '$@;)

lib_LTLIBRARIES = libgda-ui-4.0.la

SUBDIRS = marshallers data-entries internal data . data-entries/plugins demos

AM_CPPFLAGS = \
	-I$(top_builddir)/ui \
	-I$(top_srcdir)/ui \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-DPLUGINSDIR=\""$(top_builddir)/libgda-ui/plugins/.libs"\" \
	-DGDA_ABI_VERSION=\""$(GDA_ABI_VERSION)"\" \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS)

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
	gdaui-server-operation.c \
	gdaui-set.h \
	gdaui-set.c \
	gdaui-tree-store.c \
	gdaui-init.c

libgda_ui_4_0_la_SOURCES = \
	$(libgda_ui_built_cfiles) \
	$(ui_sources)

gdaui-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(ui_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#ifndef __LIBGDAUI_ENUM_TYPES_H__\n#define __LIBGDAUI_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDAUI_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBGDAUI_ENUM_TYPES_H__ */" \
		$(ui_headers) ) > tmp-gdaui-enum-types.h \
	&& (cmp -s tmp-gdaui-enum-types.h gdaui-enum-types.h || cp tmp-gdaui-enum-types.h gdaui-enum-types.h ) \
	&& rm -f tmp-gdaui-enum-types.h	\
	&& echo timestamp > $(@F)

gdaui-enum-types.c: s-enum-types-c
	@true

s-enum-types-c: @REBUILD@ $(ui_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#include <libgda-ui/libgda-ui.h>\n" \
	                --fhead "#include <glib-object.h>" \
                        --fprod "\n/* enumerations from \"@filename@\" */" \
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

$(OBJECTS) $(libgda_ui_4_0_la_OBJECTS): $(libgda_ui_built_headers) $(libgda_ui_built_cfiles)

libgda_ui_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	$(NO_UNDEFINED) $(LIBTOOL_UI_EXPORT_OPTIONS)
libgda_ui_4_0_la_LIBADD = \
	marshallers/libgda-ui-marshallers.la \
	internal/libgda-ui-internal.la \
	data-entries/libgda-ui-data-entries.la \
	../libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(GTK_LIBS) \
	$(GIO_LIBS)

if PLATFORM_WIN32
libgda_ui_4_0_la_LDFLAGS += -export-symbols $(builddir)/libgda-ui.def
libgda_ui_4_0_la_DEPENDENCIES = libgda-ui.def

libgda-ui.def: libgda-ui.symbols
	(echo -e EXPORTS; $(CPP) -P - <$(srcdir)/libgda-ui.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgda-ui.def.tmp && mv libgda-ui.def.tmp libgda-ui.def

libgda-ui-$(GDA_ABI_VERSION).lib: libgda-ui.def
	$(DLLTOOL) --dllname libgda-ui-$(GDA_ABI_VERSION)-$(GDA_CURRENT).dll --def libgda-ui.def --output-lib libgda-ui-$(GDA_ABI_VERSION).lib

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
	libgda-ui-$(GDA_ABI_VERSION).lib

DTD_FILES = gdaui-layout.dtd
dtddir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/dtd
dtd_DATA=$(DTD_FILES)

EXTRA_DIST = $(DTD_FILES) libgda-ui.symbols

if HAVE_INTROSPECTION
BUILT_GIRSOURCES =
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

plugin_sources = \
        data-entries/plugins/common-pict.c \
        data-entries/plugins/common-pict.h \
        data-entries/plugins/custom-marshal.c \
        data-entries/plugins/custom-marshal.h \
        data-entries/plugins/gdaui-data-cell-renderer-cgrid.c \
        data-entries/plugins/gdaui-data-cell-renderer-cgrid.h \
        data-entries/plugins/gdaui-data-cell-renderer-password.c \
        data-entries/plugins/gdaui-data-cell-renderer-password.h \
        data-entries/plugins/gdaui-data-cell-renderer-pict.c \
        data-entries/plugins/gdaui-data-cell-renderer-pict.h \
        data-entries/plugins/gdaui-entry-cgrid.c \
        data-entries/plugins/gdaui-entry-cgrid.h \
        data-entries/plugins/gdaui-entry-cidr.c \
        data-entries/plugins/gdaui-entry-cidr.h \
        data-entries/plugins/gdaui-entry-filesel.c \
        data-entries/plugins/gdaui-entry-filesel.h \
        data-entries/plugins/gdaui-entry-password.c \
        data-entries/plugins/gdaui-entry-password.h \
        data-entries/plugins/gdaui-entry-pict.c \
        data-entries/plugins/gdaui-entry-pict.h \
        data-entries/plugins/gdaui-entry-text.c \
        data-entries/plugins/gdaui-entry-text.h

gir_sources = \
	$(ui_headers) \
	$(ui_sources) \
	$(plugin_sources) \
	$(ui_data_entries_sources)

Gdaui-@GDA_ABI_VERSION@.gir: $(INTROSPECTION_SCANNER) libgda-ui-4.0.la
	$(INTROSPECTION_SCANNER) -v \
		--namespace Gdaui --nsversion=@GDA_ABI_VERSION@ \
		-I$(top_srcdir) -I$(top_srcdir)/libgda -I$(top_srcdir)/libgda-ui \
		$(LIBGDA_CFLAGS) \
		$(GTK_CFLAGS) \
		$(GIO_CFLAGS) \
		--add-include-path=$(top_builddir)/libgda \
		--include=GL-1.0 \
		--include=GObject-2.0 \
		--include=libxml2-2.0 \
		--include=Gtk-2.0 \
		--include=Gda-4.0 \
		--library=gda-ui-@GDA_ABI_VERSION@ \
	    $(INCLUDE_CFLAGS) \
	    --output $@ \
		$(addprefix $(srcdir)/, $(ui_sources)) \
		$(addprefix $(srcdir)/, $(ui_data_entries_sources)) 

BUILT_GIRSOURCES += Gdaui-@GDA_ABI_VERSION@.gir

endif

girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/

typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

Gdaui-@GDA_ABI_VERSION@.typelib: Gdaui-@GDA_ABI_VERSION@.gir $(INTROSPECTION_COMPILER)
	$(QUIET_GEN)$(DEBUG) $(INTROSPECTION_COMPILER) \
	--includedir=$(srcdir) --includedir=. \
	--includedir=$(top_builddir)/libgda \
	$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

CLEANFILES += $(BUILT_GIRSOURCES) $(typelibs_DATA)

