lib_LTLIBRARIES = libgda-2.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DLIBGDA_PLUGINDIR=\""$(prefix)/lib/libgda/providers"\" \
	-DLIBGDA_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBGDA_GLOBAL_CONFIG_FILE=\""$(sysconfdir)/libgda/config"\"

gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=gda_marshal > $@

gda_headers = \
	gda-client.h \
	gda-command.h \
	gda-config.h \
	gda-connection.h \
	gda-data-model.h \
	gda-data-model-array.h \
	gda-data-model-hash.h \
	gda-data-model-list.h \
	gda-error.h \
	gda-export.h \
	gda-field.h \
	gda-log.h \
	gda-marshal.h \
	gda-parameter.h \
	gda-quark-list.h \
	gda-row.h \
	gda-select.h \
	gda-server-provider.h \
	gda-table.h \
	gda-transaction.h \
	gda-util.h \
	gda-value.h \
	gda-xml-connection.h \
	gda-xml-database.h \
	gda-xml-document.h \
	gda-xql-atom.h \
	gda-xql-dual.h \
	gda-xql-list.h \
	gda-xql-utils.h \
	gda-xql-bin.h \
	gda-xql-field.h \
	gda-xql-query.h \
	gda-xql-value.h \
	gda-xql-column.h \
	gda-xql-func.h \
	gda-xql-select.h \
	gda-xql-valueref.h \
	gda-xql-const.h \
	gda-xql-insert.h \
	gda-xql-stack.h \
	gda-xql-delete.h \
	gda-xql-item.h \
	gda-xql-target.h \
	gda-xql-dml.h \
	gda-xql-join.h \
	gda-xql-update.h \
	libgda.h

$(libgda_2_la_OBJECTS): $(gda_marshal_built_files)

libgda_2_la_SOURCES = \
	$(gda_headers) \
	gda-client.c \
	gda-command.c \
	gda-config.c \
	gda-connection.c \
	gda-data-model.c \
	gda-data-model-array.c \
	gda-data-model-hash.c \
	gda-data-model-list.c \
	gda-error.c \
	gda-export.c \
	gda-field.c \
	gda-init.c \
	gda-intl.h \
	gda-log.c \
	gda-parameter.c \
	gda-quark-list.c \
	gda-row.c \
	gda-select.c \
	gda-server-provider.c \
	gda-table.c \
	gda-transaction.c \
	gda-util.c \
	gda-value.c \
	gda-xml-connection.c \
	gda-xml-database.c \
	gda-xml-document.c \
	gda-xql-atom.c \
	gda-xql-dual.c \
	gda-xql-list.c \
	gda-xql-utils.c \
	gda-xql-bin.c \
	gda-xql-field.c \
	gda-xql-query.c \
	gda-xql-value.c \
	gda-xql-column.c \
	gda-xql-func.c \
	gda-xql-select.c \
	gda-xql-valueref.c \
	gda-xql-const.c \
	gda-xql-insert.c \
	gda-xql-stack.c \
	gda-xql-delete.c \
	gda-xql-item.c \
	gda-xql-target.c \
	gda-xql-dml.c \
	gda-xql-join.c \
	gda-xql-update.c

libgda_2_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_2_la_LIBADD = $(LIBGDA_LIBS) 

gdaincludedir=$(includedir)/libgda
gdainclude_HEADERS=$(gda_headers)

CLEANFILES = $(gda_marshal_built_files)

dist-hook:
	cd $(distdir); rm -f $(gda_marshal_built_files)

EXTRA_DIST = \
	gda-marshal.list
