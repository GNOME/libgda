lib_LTLIBRARIES = libgda-common-2.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	-I.

BUILT_SOURCES = GDA-skels.c GDA-stubs.c GDA-common.c GDA.h
CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/idl/GDA.idl \
	$(top_builddir)/idl/GDA_Command.idl \
	$(top_builddir)/idl/GDA_Connection.idl \
	$(top_builddir)/idl/GDA_Error.idl \
	$(top_builddir)/idl/GDA_Field.idl \
	$(top_builddir)/idl/GDA_Listener.idl \
	$(top_builddir)/idl/GDA_Parameter.idl \
	$(top_builddir)/idl/GDA_Recordset.idl

GDA-skels.c GDA-stubs.c GDA-common.c GDA.h: $(IDLFILES) $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_builddir)/idl -I$(BONOBO_ACTIVATION_IDL_DIR) $(top_builddir)/idl/GDA.idl

gda_common_headers = \
	gda-common-defs.h \
	gda-config.h \
	gda-corba.h \
	gda-error.h \
	gda-listener.h \
	gda-log.h \
	gda-quark-list.h \
	gda-thread.h \
	gda-util.h \
	gda-xml-atom-item.h \
	gda-xml-bin-item.h \
	gda-xml-database.h \
	gda-xml-document.h \
	gda-xml-item.h \
	gda-xml-list-item.h \
	gda-xml-util.h \
	gda-common.h \
	GDA.h

libgda_common_2_la_SOURCES = \
	$(gda_common_headers) \
	gda-common-private.h \
	GDA-common.c \
	GDA-skels.c \
	GDA-stubs.c \
	gda-config.c \
	gda-corba.c \
	gda-error.c \
	gda-init.c \
	gda-listener.c \
	gda-log.c \
	gda-quark-list.c \
	gda-thread.c \
	gda-util.c \
	gda-xml-atom-item.c \
	gda-xml-bin-item.c \
	gda-xml-database.c \
	gda-xml-document.c \
	gda-xml-item.c \
	gda-xml-list-item.c \
	gda-xml-util.c

libgda_common_2_la_LIBADD=\
	$(GDA_CLIENT_LIBS) 

gdaincludedir=$(includedir)/gnome/2/gda
gdainclude_HEADERS=$(gda_common_headers)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST = \
	gda-xml-query.dtd \
	gda-xml-query.xsl
