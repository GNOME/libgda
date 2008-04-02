lib_LTLIBRARIES = libgda-4.0.la

SUBDIRS = sqlite handlers binreloc sql-parser providers-support

if BDB 
GDA_BDB_H=gda-data-model-bdb.h
GDA_BDB_S=gda-data-model-bdb.c
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(FAM_CFLAGS) \
	$(BDB_CFLAGS) \
	$(GNOMEVFS_CFLAGS)

gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=gda_marshal > $@

gda_headers = \
	gda-batch.h \
	gda-blob-op.h \
	gda-column.h \
	gda-config.h \
	gda-connection-event.h \
	gda-connection.h \
	gda-connection-private.h \
	gda-data-comparator.h \
	gda-data-handler.h \
	gda-data-model-array.h \
	gda-data-model-row.h \
	gda-data-model.h \
	$(GDA_BDB_H) \
	gda-data-model-dir.h \
	gda-data-model-extra.h \
	gda-data-model-import.h \
	gda-data-model-iter.h \
	gda-data-model-private.h \
	gda-data-model-query.h \
	gda-data-access-wrapper.h \
	gda-data-proxy.h \
	gda-debug-macros.h \
	gda-decl.h \
	gda-easy.h \
	gda-enums.h \
	gda-holder.h \
	gda-log.h \
	gda-marshal.h \
	gda-meta-store.h \
	gda-meta-struct.h \
	gda-quark-list.h \
	gda-set.h \
	gda-row.h \
	gda-server-operation.h \
	gda-server-provider.h \
	gda-server-provider-extra.h \
	gda-server-provider-private.h \
	gda-statement.h \
	gda-statement-extra.h \
	gda-threader.h \
	gda-transaction-status.h \
	gda-transaction-status-private.h \
	gda-util.h \
	gda-value.h \
	libgda.h 

libgda_sources =  \
	csv.h \
	libcsv.c \
	gda-batch.c \
	dir-blob-op.h \
	dir-blob-op.c \
	$(gda_headers) \
	gda-blob-op.c \
	gda-column.c \
	gda-config.c \
	gda-connection.c \
	gda-connection-event.c \
	gda-data-comparator.c \
	gda-data-handler.c \
	gda-data-model-array.c \
	$(GDA_BDB_S) \
	gda-data-model-row.c \
	gda-data-model.c \
	gda-data-model-dir.c \
	gda-data-model-dsn-list.c \
	gda-data-model-dsn-list.h \
	gda-data-model-import.c \
	gda-data-model-iter.c \
	gda-data-model-query.c \
	gda-data-access-wrapper.c \
	gda-data-proxy.c \
	gda-easy.c \
	gda-holder.c \
	gda-init.c \
	gda-log.c \
	gda-marshal.c \
	gda-meta-store.c \
	gda-meta-store-extra.h \
	gda-meta-struct.c \
	gda-quark-list.c \
	gda-set.c \
	gda-row.c \
	gda-server-operation.c \
	gda-server-provider.c \
	gda-server-provider-extra.c \
	gda-statement.c \
	gda-threader.c \
	gda-transaction-status.c \
	gda-util.c \
	gda-value.c \
	global.h \
	md5.h \
	md5c.c

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
$(OBJECTS) $(libgda_4_0_la_OBJECTS): gda-marshal.c $(libgda_built_headers) $(libgda_built_cfiles)

libgda_4_0_la_SOURCES = \
	$(libgda_built_cfiles) \
	$(libgda_sources)

libgda_4_0_la_LDFLAGS = \
	-version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	$(NO_UNDEFINED) \
	-export-dynamic $(no_undefined) $(LIBTOOL_EXPORT_OPTIONS)

libgda_4_0_la_LIBADD = \
	sql-parser/libgda_sql_parser-4.0.la \
	providers-support/libgda-psupport.la \
	handlers/libgda_handlers-4.0.la \
	binreloc/libgda_binreloc-4.0.la \
	sqlite/libgda-sqlite.la \
	$(LIBGDA_LIBS) $(FAM_LIBS) \
	$(BDB_LIBS) $(GNOMEVFS_LIBS) 

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda
gdainclude_HEADERS=$(libgda_built_headers) $(gda_headers)

CLEANFILES = $(libgda_built_headers) $(libgda_built_cfiles) \
	gda-marshal.c \
	gda-enum-types.h \
	s-enum-types-h \
	gda-enum-types.c \
	s-enum-types-c

dist-hook:
	cd $(distdir); rm -f $(libgda_built_headers) $(libgda_built_cfiles)

xmldir   = $(datadir)/libgda-4.0
xml_DATA = information_schema.xml

DTD_FILES = libgda-array.dtd libgda-paramlist.dtd  libgda-server-operation.dtd
dtddir=$(LIBGDA_DTDDIR)
dtd_DATA=$(DTD_FILES)
EXTRA_DIST = \
	$(DTD_FILES) \
	$(xml_DATA) \
	gda-marshal.list

DISTCLEANFILES  = .deps/*.P
