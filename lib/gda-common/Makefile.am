lib_LTLIBRARIES = libgda-common-2.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	-I.

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

gda_common_headers = \
	gda-common.h \
	gda-config.h \
	gda-data-model.h \
	gda-error.h \
	gda-field.h \
	gda-parameter.h \
	gda-quark-list.h \
	gda-value.h \
	GNOME_Database.h

$(libgda_common_2_la_OBJECTS): $(gda_marshal_built_files)

libgda_common_2_la_SOURCES = \
	$(gda_common_headers) \
	GNOME_Database-common.c \
	GNOME_Database-skels.c \
	GNOME_Database-stubs.c \
	gda-config.c \
	gda-data-model.c \
	gda-error.c \
	gda-field.c \
	gda-init.c \
	gda-parameter.c \
	gda-quark-list.c \
	gda-value.c

libgda_common_2_la_LIBADD=\
	$(GDA_CLIENT_LIBS) 

gdaincludedir=$(includedir)/libgda-$(VERSION)/gda
gdainclude_HEADERS=$(gda_common_headers)

CLEANFILES = $(gda_marshal_built_files) $(gda_corba_built_sources)

dist-hook:
	cd $(distdir); rm -f $(gda_marshal_built_files) $(gda_corba_built_sources)

EXTRA_DIST = \
	gda-marshal.list \
	gda-xml-query.dtd \
	gda-xml-query.xsl
