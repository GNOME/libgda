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

libgda_ui_4_0_la_SOURCES = \
	$(ui_headers) \
	$(libgda_ui_built_cfiles) \
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
libgda_ui_4_0_la_LDFLAGS += -export-symbols $(srcdir)/libgda-ui.def
libgda_ui_4_0_la_DEPENDENCIES = libgda-ui.def

libgda-ui.def: libgda-ui.symbols
	(echo -e EXPORTS; $(CPP) -P - <$(srcdir)/libgda-ui.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgda-ui.def.tmp && mv libgda-ui.def.tmp libgda-ui.def

libgda-ui-$(GDA_ABI_VERSION).lib: libgda-ui.def
	$(DLLTOOL) --dllname libgda-ui-$(GDA_ABI_VERSION)-$(GDA_CURRENT).dll --def libgda-ui.def --output-lib libgda-ui-$(GDA_ABI_VERSION).lib

install-data-local: install-def-files
uninstall-local: uninstall-def-files

install-def-files: libgda-ui.def libgda-ui-$(GDA_ABI_VERSION).lib
	$(INSTALL) $(srcdir)/libgda-ui.def $(DESTDIR)$(libdir)/libgda-ui-$(GDA_ABI_VERSION).def
	$(INSTALL) $(srcdir)/libgda-ui-$(GDA_ABI_VERSION).lib $(DESTDIR)$(libdir)/

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

DTD_FILES = data-layout.dtd
dtddir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/dtd
dtd_DATA=$(DTD_FILES)

EXTRA_DIST = $(DTD_FILES) libgda-ui.symbols
