lib_LTLIBRARIES = libgda-3.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DLIBGDA_PLUGINDIR=\""$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers"\" \
	-DLIBGDA_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBGDA_GLOBAL_CONFIG_FILE=\""$(sysconfdir)/libgda/config"\" \
	-DLIBGDA_GLOBAL_CONFIG_DIR=\""$(sysconfdir)/libgda"\"

gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=gda_marshal > $@

gda_headers = \
	gda-blob.h \
	gda-client.h \
	gda-command.h \
	gda-config.h \
	gda-connection.h \
	gda-data-model.h \
	gda-data-model-array.h \
	gda-data-model-base.h \
	gda-data-model-hash.h \
	gda-data-model-list.h \
	gda-connection-event.h \
	gda-column.h \
	gda-column-index.h \
	gda-data-model-index.h \
	gda-log.h \
	gda-parameter.h \
	gda-quark-list.h \
	gda-row.h \
	gda-select.h \
	gda-server-provider.h \
	gda-transaction.h \
	gda-util.h \
	gda-threader.h \
	gda-value.h \
	global-decl.h \
	libgda.h

libgda_sources =  \
	$(gda_headers) \
	gda-blob.c \
	gda-client.c \
	gda-command.c \
	gda-config.c \
	gda-connection.c \
	gda-data-model.c \
	gda-data-model-array.c \
	gda-data-model-base.c \
	gda-data-model-hash.c \
	gda-data-model-list.c \
	gda-data-model-private.h \
	gda-connection-event.c \
	gda-column.c \
	gda-column-index.c \
	gda-data-model-index.c \
	gda-init.c \
	gda-intl.h \
	gda-log.c \
	gda-log.h \
	gda-parameter.c \
	gda-quark-list.c \
	gda-row.c \
	gda-select.c \
	gda-server-provider.c \
	gda-transaction.c \
	gda-util.c \
	gda-threader.c \
	gda-value.c 

gda-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(gda_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#ifndef __LIBGDA_ENUM_TYPES_H__\n#define __LIBGDA_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDA_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBGDA_ENUM_TYPES_H__ */" \
		$(gda_headers) ) > tmp-gda-enum-types.h \
	&& (cmp -s tmp-gda-enum-types.h gda-enum-types.h || cp tmp-gda-enum-types.h gda-enum-types.h ) \
	&& rm -f tmp-gda-enum-types.h	\
	&& echo timestamp > $(@F)

gda-enum-types.c: s-enum-types-c
	@true

s-enum-types-c: @REBUILD@ $(gda_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#include <libgda/libgda.h>\n" \
	                --fhead "#include <glib-object.h>" \
                        --fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gda_headers) ) > tmp-gda-enum-types.c \
	&& (cmp -s tmp-gda-enum-types.c gda-enum-types.c || cp tmp-gda-enum-types.c gda-enum-types.c ) \
	&& rm -f tmp-gda-enum-types.c \
	&& echo timestamp > $(@F)


# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
libgda_built_headers = gda-marshal.h gda-enum-types.h
libgda_built_cfiles = gda-enum-types.c

# Don't build the library until we have built the headers that it needs:
$(OBJECTS) $(libgda_3_la_OBJECTS): gda-marshal.c $(libgda_built_headers) $(libgda_built_cfiles)

libgda_3_la_SOURCES = \
	$(libgda_built_cfiles) \
	$(libgda_sources)

libgda_3_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_3_la_LIBADD = $(LIBGDA_LIBS) $(FAM_LIBS) $(top_builddir)/libsql/libgdasql.la

gdaincludedir=$(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/libgda
gdainclude_HEADERS=$(libgda_built_headers) $(gda_headers)

CLEANFILES = $(libgda_built_headers) $(libgda_built_cfiles) \
	gda-marshal.c \
	gda-enum-types.h \
	s-enum-types-h \
	gda-enum-types.c \
	s-enum-types-c

dist-hook:
	cd $(distdir); rm -f $(libgda_built_headers) $(libgda_built_cfiles)

EXTRA_DIST = \
	gda-marshal.list
