lib_LTLIBRARIES = libgdaui-4.0.la

SUBDIRS = marshallers data-entries internal data . data-entries/plugins demos

AM_CPPFLAGS = \
	-I$(top_builddir)/ui \
	-I$(top_srcdir)/ui \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-DPLUGINSDIR=\""$(top_builddir)/libgdaui/plugins/.libs"\" \
	-DGDA_ABI_VERSION=\""$(GDA_ABI_VERSION)"\" \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS)

ui_headers = \
	gdaui-basic-form.h \
	gdaui-combo.h \
	gdaui-data-entry.h \
	gdaui-data-store.h \
	gdaui-data-widget.h \
	gdaui-data-widget-filter.h \
	gdaui-data-widget-info.h \
	gdaui-decl.h \
	gdaui-easy.h \
	gdaui-enums.h \
	gdaui-form.h \
	gdaui-grid.h \
	gdaui-login.h \
	gdaui-provider-selector.h \
	gdaui-raw-form.h \
	gdaui-raw-grid.h \
	gdaui-server-operation.h \
	gdaui-set.h \
	gdaui-tree-store.h

libgdaui_4_0_la_SOURCES = \
	$(ui_headers) \
	$(libgdaui_built_cfiles) \
	gdaui-basic-form.c \
	gdaui-combo.c \
	gdaui-data-entry.c \
	gdaui-data-store.c \
	gdaui-data-widget.c \
	gdaui-data-widget-filter.c \
	gdaui-data-widget-info.c \
	gdaui-form.c \
	gdaui-grid.c \
	gdaui-login.c \
	gdaui-provider-selector.c \
	gdaui-raw-form.c \
	gdaui-raw-grid.c \
	gdaui-server-operation.c \
	gdaui-set.c \
	gdaui-tree-store.c \
	gdaui-init.c \
	gdaui-plugin.h

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
	                --fhead "#include <libgdaui/libgdaui.h>\n" \
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
libgdaui_built_headers = gdaui-enum-types.h
libgdaui_built_cfiles = gdaui-enum-types.c

$(OBJECTS) $(libgdaui_4_0_la_OBJECTS): $(libgdaui_built_headers) $(libgdaui_built_cfiles)

libgdaui_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	$(NO_UNDEFINED) $(LIBTOOL_UI_EXPORT_OPTIONS)
libgdaui_4_0_la_LIBADD = \
	marshallers/libgdaui-marshallers.la \
	internal/libgdaui-internal.la \
	data-entries/libgdaui-data-entries.la \
	../libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(GTK_LIBS)

if PLATFORM_WIN32
libgdaui_4_0_la_LDFLAGS += -export-symbols $(srcdir)/libgdaui.def
libgdaui_4_0_la_DEPENDENCIES = libgdaui.def

libgdaui.def: libgdaui.symbols
	(echo -e EXPORTS; $(CPP) -P - <$(srcdir)/libgdaui.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgdaui.def.tmp && mv libgdaui.def.tmp libgdaui.def

libgdaui-$(GDA_ABI_VERSION).lib: libgdaui.def
	$(DLLTOOL) --dllname libgdaui-$(GDA_ABI_VERSION)-$(GDA_CURRENT).dll --def libgdaui.def --output-lib libgdaui-$(GDA_ABI_VERSION).lib

install-data-local: install-def-files
uninstall-local: uninstall-def-files

install-def-files: libgdaui.def libgdaui-$(GDA_ABI_VERSION).lib
	$(INSTALL) $(srcdir)/libgdaui.def $(DESTDIR)$(libdir)/libgdaui-$(GDA_ABI_VERSION).def
	$(INSTALL) $(srcdir)/libgdaui-$(GDA_ABI_VERSION).lib $(DESTDIR)$(libdir)/

uninstall-def-files:
	-rm $(DESTDIR)$(libdir)/libgdaui-$(GDA_ABI_VERSION).def
	-rm $(DESTDIR)$(libdir)/libgdaui-$(GDA_ABI_VERSION).lib
endif

gdauiincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgdaui
gdauiinclude_HEADERS= libgdaui.h $(ui_headers) $(libgdaui_built_headers)

CLEANFILES = $(libgdaui_built_headers) $(libgdaui_built_cfiles) \
	gdaui-enum-types.h \
	s-enum-types-h \
	gdaui-enum-types.c \
	s-enum-types-c \
	libgdaui.def \
	libgdaui-$(GDA_ABI_VERSION).lib

DTD_FILES = data-layout.dtd
dtddir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/dtd
dtd_DATA=$(DTD_FILES)

EXTRA_DIST = $(DTD_FILES) libgdaui.symbols
