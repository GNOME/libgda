lib_LTLIBRARIES = libgda-server.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common

BUILT_SOURCES = GDA-skels.c GDA-stubs.c GDA-common.c GDA.h
CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/idl/GDA.idl \
	$(top_builddir)/idl/GDA_Command.idl \
	$(top_builddir)/idl/GDA_Connection.idl \
	$(top_builddir)/idl/GDA_Error.idl \
	$(top_builddir)/idl/GDA_Fieldx.idl \
	$(top_builddir)/idl/GDA_Parameter.idl \
	$(top_builddir)/idl/GDA_Recordset.idl

GDA-skels.c GDA-stubs.c GDA-common.c GDA.h: $(IDLFILES)
	$(ORBIT_IDL) -I$(top_sourcedir)/idl -I`oaf-config --prefix`/share/idl $(top_builddir)/idl/GDA.idl

gda_server_headers = \
	gda-server-impl.h \
	gda-server.h \
	GDA.h

libgda_server_la_SOURCES = \
	GDA.h \
	GDA-common.c \
	GDA-skels.c \
	gda-server-command.c \
	gda-server-connection.c \
	gda-server-error.c \
	gda-server-field.c \
	gda-server-impl-command.c \
	gda-server-impl-command.h \
	gda-server-impl-connection.c \
	gda-server-impl-connection.h \
	gda-server-impl-factory.c \
	gda-server-impl-factory.h \
	gda-server-impl-recordset.c \
	gda-server-impl-recordset.h \
	gda-server-impl.c \
	gda-server-init.c \
	gda-server-recordset.c \
	$(gda_server_headers)
libgda_server_la_LIBADD = -lpopt

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_server_headers)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
