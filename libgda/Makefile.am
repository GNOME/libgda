QUIET_GEN = $(Q:@=@echo ' GEN '$@;)

lib_LTLIBRARIES = libgda-5.0.la

SUBDIRS = sqlite handlers binreloc sql-parser providers-support thread-wrapper

if BDB 
GDA_BDB_H=gda-data-model-bdb.h
GDA_BDB_S=gda-data-model-bdb.c
DEF_FLAGS=-DHAVE_BDB
endif

GLOBAL_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-DABI_VERSION=\""$(GDA_ABI_VERSION)"\" \
	$(LIBGDA_CFLAGS) \
	$(BDB_CFLAGS) \
	-DBDB_LIB=\""$(BDB_LIB)"\" \
	$(GIO_CFLAGS) $(GNOME_KEYRING_CFLAGS)

AM_CPPFLAGS = \
	$(GLOBAL_CFLAGS) \
	$(LIBGDA_WFLAGS)

_gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=_gda_marshal > $@

mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -g -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/libgda/keywords.list
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/libgda/keywords.list > keywords_hash.c

gda_headers = \
	gda-attributes-manager.h \
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
	gda-data-model.h \
	$(GDA_BDB_H) \
	gda-data-model-dir.h \
	gda-data-model-extra.h \
	gda-data-model-import.h \
	gda-data-model-iter.h \
	gda-data-model-iter-extra.h \
	gda-data-model-private.h \
	gda-data-access-wrapper.h \
	gda-data-proxy.h \
	gda-data-select.h \
	gda-debug-macros.h \
	gda-decl.h \
	gda-enums.h \
	gda-holder.h \
	gda-lockable.h \
	gda-log.h \
	gda-meta-store.h \
	gda-meta-struct.h \
	gda-mutex.h \
	gda-quark-list.h \
	gda-repetitive-statement.h \
	gda-row.h \
	gda-set.h \
	gda-server-operation.h \
	gda-server-provider.h \
	gda-server-provider-extra.h \
	gda-server-provider-private.h \
	gda-statement.h \
	gda-statement-extra.h \
	gda-sql-builder.h \
	gda-transaction-status.h \
	gda-transaction-status-private.h \
	gda-tree.h \
	gda-tree-mgr-columns.h \
	gda-tree-mgr-schemas.h \
	gda-tree-mgr-tables.h \
	gda-tree-node.h \
	gda-tree-manager.h \
	gda-tree-mgr-label.h \
	gda-tree-mgr-select.h \
	gda-util.h \
	gda-value.h \
	gda-xa-transaction.h \
	libgda-global-variables.h

gda_built_sources= \
	$(builddir)/libgda.h

gda_sources= \
	gda-batch.c \
	gda-blob-op.c \
	gda-column.c \
	gda-config.c \
	gda-connection.c \
	gda-connection-event.c \
	gda-data-comparator.c \
	gda-data-handler.c \
	gda-data-model-array.c \
	$(GDA_BDB_S) \
	gda-data-model.c \
	gda-data-model-dir.c \
	gda-data-model-dsn-list.c \
	gda-data-model-dsn-list.h \
	gda-data-model-import.c \
	gda-data-model-iter.c \
	gda-data-access-wrapper.c \
	gda-data-meta-wrapper.c \
	gda-data-meta-wrapper.h \
	gda-data-proxy.c \
	gda-data-select.c \
	gda-holder.c \
	gda-init.c \
	gda-lockable.c \
	gda-log.c \
	gda-meta-store.c \
	gda-meta-struct.c \
	gda-meta-struct-io.c \
	gda-mutex.c \
	gda-quark-list.c \
	gda-repetitive-statement.c \
	gda-row.c \
	gda-set.c \
	gda-server-operation.c \
	gda-server-operation-private.h \
	gda-server-provider.c \
	gda-server-provider-extra.c \
	gda-statement.c \
	gda-sql-builder.c \
	gda-transaction-status.c \
	gda-tree.c \
	gda-tree-mgr-columns.c \
	gda-tree-mgr-schemas.c \
	gda-tree-mgr-tables.c \
	gda-tree-node.c \
	gda-tree-manager.c \
	gda-tree-mgr-label.c \
	gda-tree-mgr-select.c \
	gda-util.c \
	gda-value.c \
	gda-xa-transaction.c 

libgda_sources =  \
	csv.h \
	libcsv.c \
	dir-blob-op.h \
	dir-blob-op.c \
	gda-attributes-manager.c \
	$(gda_headers) \
	gda-connection-internal.h \
	gda-connection-sqlite.h \
	gda-custom-marshal.c \
	gda-custom-marshal.h \
	gda-data-select-extra.h \
	gda-types.c \
	gda-types.h \
	gda-marshal.c \
	gda-meta-store-extra.h \
	gda-meta-struct-private.h \
	gda-statement-priv.h \
	global.h \
	md5.h \
	md5c.c \
	$(gda_built_sources) \
	$(gda_sources)

gda-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(gda_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
	                --fhead "#ifndef __LIBGDA_ENUM_TYPES_H__\n#define __LIBGDA_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDA_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBGDA_ENUM_TYPES_H__ */" \
		$(gda_headers)) > tmp-gda-enum-types.h \
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
		$(gda_headers)) > tmp-gda-enum-types.c \
	&& (cmp -s tmp-gda-enum-types.c gda-enum-types.c || cp tmp-gda-enum-types.c gda-enum-types.c ) \
	&& rm -f tmp-gda-enum-types.c \
	&& echo timestamp > $(@F)


# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
libgda_built_headers = gda-marshal.h gda-enum-types.h
libgda_built_cfiles = gda-enum-types.c

# Don't build the library until we have built the headers that it needs:
$(OBJECTS) $(libgda_5_0_la_OBJECTS): gda-marshal.c $(libgda_built_headers) $(libgda_built_cfiles) keywords_hash.c

libgda_5_0_la_SOURCES = \
	$(libgda_built_cfiles) \
	$(libgda_sources)

libgda_5_0_la_LDFLAGS = \
	-version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	-export-dynamic $(NO_UNDEFINED) $(LIBTOOL_EXPORT_OPTIONS) \
	$(GDA_DEBUG_LDFLAGS)

libgda_5_0_la_LIBADD = \
	sql-parser/libgda_sql_parser-5.0.la \
	providers-support/libgda-psupport.la \
	handlers/libgda_handlers-5.0.la \
	binreloc/libgda_binreloc-5.0.la \
	sqlite/libgda-sqlite.la \
	thread-wrapper/libgda_threadwrapper-5.0.la \
	$(LIBGDA_LIBS) \
	$(GIO_LIBS) $(GNOME_KEYRING_LIBS)

if PLATFORM_WIN32
libgda_5_0_la_LDFLAGS += -export-symbols $(builddir)/libgda.def
libgda_5_0_la_DEPENDENCIES = libgda.def

libgda.def: libgda.symbols
	(echo -e EXPORTS; $(CPP) -P $(DEF_FLAGS) - <$(srcdir)/libgda.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgda.def.tmp && mv libgda.def.tmp libgda.def

libgda-$(GDA_ABI_VERSION).lib: libgda.def
	$(DLLTOOL) --dllname libgda-$(GDA_ABI_VERSION)-$(GDA_ABI_MAJOR_VERSION).dll --def libgda.def --output-lib libgda-$(GDA_ABI_VERSION).lib

install-data-local: install-def-files
uninstall-local: uninstall-def-files

install-def-files: libgda.def libgda-$(GDA_ABI_VERSION).lib
	$(INSTALL) $(builddir)/libgda.def $(DESTDIR)$(libdir)/libgda-$(GDA_ABI_VERSION).def
	$(INSTALL) $(builddir)/libgda-$(GDA_ABI_VERSION).lib $(DESTDIR)$(libdir)/

uninstall-def-files:
	-rm $(DESTDIR)$(libdir)/libgda-$(GDA_ABI_VERSION).def
	-rm $(DESTDIR)$(libdir)/libgda-$(GDA_ABI_VERSION).lib
endif

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda
gdainclude_HEADERS=$(libgda_built_headers) $(gda_headers) libgda.h

CLEANFILES = $(libgda_built_headers) $(libgda_built_cfiles) \
	gda-marshal.c \
	gda-enum-types.h \
	s-enum-types-h \
	gda-enum-types.c \
	s-enum-types-c \
	libgda.def \
	libgda-$(GDA_ABI_VERSION).lib \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c

dist-hook:
	cd $(distdir); rm -f $(libgda_built_headers) $(libgda_built_cfiles)

xmldir   = $(datadir)/libgda-5.0
xml_DATA = information_schema.xml

DTD_FILES = libgda-array.dtd libgda-paramlist.dtd  libgda-server-operation.dtd
dtddir=$(LIBGDA_DTDDIR)
dtd_DATA=$(DTD_FILES)
EXTRA_DIST = \
	$(DTD_FILES) \
	$(xml_DATA) \
	gda-marshal.list \
	libgda.symbols \
	keywords.list

DISTCLEANFILES  = .deps/*.P
if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) \
			     --namespace Gda
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

psupport = \
	providers-support/gda-pstmt.h \
	providers-support/gda-pstmt.c

threadwrapper = \
	thread-wrapper/gda-thread-wrapper.h \
	thread-wrapper/gda-thread-wrapper.c

sqlparser = \
	sql-parser/gda-sql-parser.h \
	sql-parser/gda-sql-statement.h \
	sql-parser/gda-statement-struct-decl.h \
	sql-parser/gda-statement-struct.h \
	sql-parser/gda-statement-struct-compound.h \
	sql-parser/gda-statement-struct-delete.h \
	sql-parser/gda-statement-struct-update.h \
	sql-parser/gda-statement-struct-insert.h \
	sql-parser/gda-statement-struct-parts.h \
	sql-parser/gda-statement-struct-pspec.h \
	sql-parser/gda-statement-struct-select.h \
	sql-parser/gda-statement-struct-trans.h \
	sql-parser/gda-statement-struct-unknown.h \
	sql-parser/gda-statement-struct-util.h \
	sql-parser/gda-sql-parser.c \
	sql-parser/gda-statement-struct.c \
	sql-parser/gda-statement-struct-compound.c \
	sql-parser/gda-statement-struct-delete.c \
	sql-parser/gda-statement-struct-update.c \
	sql-parser/gda-statement-struct-insert.c \
	sql-parser/gda-statement-struct-parts.c \
	sql-parser/gda-statement-struct-pspec.c \
	sql-parser/gda-statement-struct-select.c \
	sql-parser/gda-statement-struct-trans.c \
	sql-parser/gda-statement-struct-unknown.c \
	sql-parser/gda-statement-struct-util.c

handlers = \
	handlers/gda-handler-bin.h \
	handlers/gda-handler-boolean.h \
	handlers/gda-handler-numerical.h \
	handlers/gda-handler-string.h \
	handlers/gda-handler-time.h \
	handlers/gda-handler-type.h \
	handlers/gda-handler-bin.c \
	handlers/gda-handler-boolean.c \
	handlers/gda-handler-numerical.c \
	handlers/gda-handler-string.c \
	handlers/gda-handler-time.c \
	handlers/gda-handler-type.c

introspection_sources = $(gda_sources) \
	$(gda_headers) \
	$(psupport) \
	$(threadwrapper) \
	$(sqlparser) \
	$(handlers)
Gda-5.0.gir: $(lib_LTLIBRARIES)
Gda_5_0_gir_INCLUDES = GObject-2.0 libxml2-2.0
Gda_5_0_gir_CFLAGS = $(GLOBAL_CFLAGS)

Gda_5_0_gir_LIBS = $(lib_LTLIBRARIES)
Gda_5_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_GIRS += Gda-5.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
CLEANFILES += $(INTROSPECTION_GIRS) $(typelibs_DATA)
endif
