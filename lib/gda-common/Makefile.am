lib_LTLIBRARIES = libgda-common.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

BUILT_SOURCES = GDA-skels.c GDA-stubs.c GDA-common.c GDA.h
CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/idl/GDA.idl \
	$(top_builddir)/idl/GDA_Command.idl \
	$(top_builddir)/idl/GDA_Connection.idl \
	$(top_builddir)/idl/GDA_Error.idl \
	$(top_builddir)/idl/GDA_Fieldx.idl \
	$(top_builddir)/idl/GDA_Listener.idl \
	$(top_builddir)/idl/GDA_Parameter.idl \
	$(top_builddir)/idl/GDA_Recordset.idl

GDA-skels.c GDA-stubs.c GDA-common.c GDA.h: $(IDLFILES) $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_sourcedir)/idl -I`oaf-config --prefix`/share/idl $(top_builddir)/idl/GDA.idl

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

libgda_common_la_SOURCES = \
	$(gda_common_headers) \
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

libgda_common_la_LIBADD=\
	$(GDA_CLIENT_LIBS) 
#	$(top_builddir)/lib/gda-common/gda-xml-query/libxmlquery.a

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_common_headers)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

EXTRA_DIST = \
	gda-xml-query.dtd \
	gda-xml-query.xsl
