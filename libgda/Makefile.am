lib_LTLIBRARIES = libgda-2.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)

BUILT_SOURCES = \
	GNOME_Database-skels.c
	GNOME_Database-stubs.c \
	GNOME_Database-common.c \
	GNOME_Database.h
CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/idl/GNOME_Database.idl

gda_corba_built_files = \
	GNOME_Database-skels.c \
	GNOME_Database-stubs.c \
	GNOME_Database-common.c \
	GNOME_Database.h

$(gda_corba_built_files): $(IDLFILES) $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_builddir)/idl -I$(BONOBO_ACTIVATION_IDL_DIR) -I$(BONOBO_IDL_DIR) $(top_builddir)/idl/GNOME_Database.idl

gda_marshal_built_files = gda-marshal.h gda-marshal.c

gda-marshal.h: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gda_marshal > $@
gda-marshal.c: gda-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=gda_marshal > $@

gda_headers = \
	GNOME_Database.h \
	gda-client.h \
	gda-command.h \
	gda-config.h \
	gda-connection.h \
	gda-data-model.h \
	gda-data-model-array.c \
	gda-data-model-list.h \
	gda-error.h \
	gda-field.h \
	gda-parameter.h \
	gda-quark-list.h \
	gda-recordset.h \
	gda-server.h \
	gda-server-connection.h \
	gda-server-provider.h \
	gda-server-recordset.h \
	gda-value.h \
	gda-xml-database.h \
	gda-xml-document.h \
	libgda.h

$(libgda_2_la_OBJECTS): $(gda_marshal_built_files)

libgda_2_la_SOURCES = \
	$(gda_headers) \
	GNOME_Database-common.c \
	GNOME_Database-skels.c \
	GNOME_Database-stubs.c \
	gda-client.c \
	gda-command.c \
	gda-config.c \
	gda-connection.c \
	gda-data-model.c \
	gda-data-model-array.c \
	gda-data-model-list.c \
	gda-error.c \
	gda-field.c \
	gda-init.c \
	gda-parameter.c \
	gda-quark-list.c \
	gda-recordset.c \
	gda-server.c \
	gda-server-connection.c \
	gda-server-provider.c \
	gda-server-recordset.c \
	gda-value.c \
	gda-xml-database.c \
	gda-xml-document.c

libgda_2_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_2_la_LIBADD = $(LIBGDA_LIBS) 

gdaincludedir=$(includedir)/gnome-2.0/libgda
gdainclude_HEADERS=$(gda_headers)

CLEANFILES = $(gda_marshal_built_files) $(gda_corba_built_sources)

dist-hook:
	cd $(distdir); rm -f $(gda_marshal_built_files) $(gda_corba_built_sources)

EXTRA_DIST = \
	gda-marshal.list
