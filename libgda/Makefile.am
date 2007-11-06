lib_LTLIBRARIES = libgda-4.0.la

SUBDIRS = sqlite sql-delimiter sql-transaction graph handlers binreloc

if BDB 
GDA_BDB_H=gda-data-model-bdb.h
GDA_BDB_S=gda-data-model-bdb.c
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	$(LIBGDA_CFLAGS) \
	$(FAM_CFLAGS) \
	$(BDB_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	$(LIBGCRYPT_CFLAGS)

gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=gda_marshal > $@

gda_headers = \
	gda-blob-op.h \
	gda-client.h \
	gda-column.h \
	gda-column-index.h \
	gda-command.h \
	gda-config.h \
	gda-connection-event.h \
	gda-connection.h \
	gda-connection-private.h \
	gda-data-handler.h \
	gda-data-model-array.h \
	gda-data-model-row.h \
	gda-data-model.h \
	$(GDA_BDB_H) \
	gda-data-model-dir.h \
	gda-data-model-extra.h \
	gda-data-model-filter-sql.h \
	gda-data-model-hash.h \
	gda-data-model-import.h \
	gda-data-model-index.h \
	gda-data-model-iter.h \
	gda-data-model-private.h \
	gda-data-model-query.h \
	gda-data-access-wrapper.h \
	gda-data-proxy.h \
	gda-decl.h \
	gda-dict-aggregate.h \
	gda-dict-constraint.h \
	gda-dict-database.h \
	gda-dict-field.h \
	gda-dict-function.h \
	gda-dict.h \
	gda-dict-private.h \
	gda-dict-reg-queries.h \
	gda-dict-reg-types.h \
	gda-dict-reg-aggregates.h \
	gda-dict-reg-functions.h \
	gda-dict-table.h \
	gda-dict-type.h \
	gda-entity-field.h \
	gda-entity.h \
	gda-enums.h \
	gda-graphviz.h \
	gda-log.h \
	gda-marshal.h \
	gda-object.h \
	gda-object-ref.h \
	gda-parameter.h \
	gda-parameter-list.h \
	gda-parameter-util.h \
	gda-quark-list.h \
	gda-query-condition.h \
	gda-query-field-agg.h \
	gda-query-field-all.h \
	gda-query-field-field.h \
	gda-query-field-func.h \
	gda-query-field.h \
	gda-query-field-value.h \
	gda-query.h \
	gda-query-join.h \
	gda-query-object.h \
	gda-query-parsing.h \
	gda-query-private.h \
	gda-query-target.h \
	gda-referer.h \
	gda-renderer.h \
	gda-row.h \
	gda-data-model-filter-sql.h \
	gda-server-operation.h \
	gda-server-provider.h \
	gda-server-provider-extra.h \
	gda-server-provider-private.h \
	gda-threader.h \
	gda-transaction-status.h \
	gda-transaction-status-private.h \
	gda-util.h \
	gda-value.h \
	gda-xml-storage.h \
	libgda.h 

libgda_sources =  \
	csv.h \
	libcsv.c \
	dir-blob-op.h \
	dir-blob-op.c \
	$(gda_headers) \
	gda-blob-op.c \
	gda-client.c \
	gda-column.c \
	gda-column-index.c \
	gda-command.c \
	gda-config.c \
	gda-connection.c \
	gda-connection-event.c \
	gda-data-handler.c \
	gda-data-model-array.c \
	$(GDA_BDB_S) \
	gda-data-model-row.c \
	gda-data-model.c \
	gda-data-model-dir.c \
	gda-data-model-dsn-list.c \
	gda-data-model-dsn-list.h \
	gda-data-model-filter-sql.c \
	gda-data-model-hash.c \
	gda-data-model-import.c \
	gda-data-model-index.c \
	gda-data-model-iter.c \
	gda-data-model-query.c \
	gda-data-access-wrapper.c \
	gda-data-proxy.c \
	gda-dict-aggregate.c \
	gda-dict.c \
	gda-dict-constraint.c \
	gda-dict-database.c \
	gda-dict-field.c \
	gda-dict-function.c \
	gda-dict-table.c \
	gda-dict-type.c \
	gda-dict-reg-queries.c \
	gda-dict-reg-types.c \
	gda-dict-reg-aggregates.c \
	gda-dict-reg-functions.c \
	gda-entity.c \
	gda-entity-field.c \
	gda-graphviz.c \
	gda-init.c \
	gda-log.c \
	gda-marshal.c \
	gda-object.c \
	gda-object-ref.c \
	gda-parameter.c \
	gda-parameter-list.c \
	gda-quark-list.c \
	gda-query.c \
	gda-query-condition.c \
	gda-query-field-agg.c \
	gda-query-field-all.c \
	gda-query-field.c \
	gda-query-field-field.c \
	gda-query-field-func.c \
	gda-query-field-value.c \
	gda-query-join.c \
	gda-query-object.c \
	gda-query-parsing.c \
	gda-query-target.c \
	gda-referer.c \
	gda-renderer.c \
	gda-row.c \
	gda-server-operation.c \
	gda-server-provider.c \
	gda-server-provider-extra.c \
	gda-threader.c \
	gda-transaction-status.c \
	gda-util.c \
	gda-value.c \
	gda-xml-storage.c 

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

libgda_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_4_0_la_LIBADD = sql-transaction/libgda_sql_transaction-4.0.la \
	sql-delimiter/libgda_sql_delimiter-4.0.la \
	graph/libgda_graph-4.0.la \
	handlers/libgda_handlers-4.0.la \
	binreloc/libgda_binreloc-4.0.la \
	sqlite/libgda-sqlite.la \
	$(top_builddir)/libsql/libgdasql-4.0.la \
	$(LIBGDA_LIBS) $(FAM_LIBS) \
	$(BDB_LIBS) $(GNOMEVFS_LIBS) $(LIBGCRYPT_LIBS)

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

DTD_FILES = libgda-dict.dtd libgda-array.dtd libgda-paramlist.dtd  libgda-server-operation.dtd
dtddir=$(LIBGDA_DTDDIR)
dtd_DATA=$(DTD_FILES)
EXTRA_DIST = \
	$(DTD_FILES) \
	gda-marshal.list

DISTCLEANFILES  = .deps/*.P
