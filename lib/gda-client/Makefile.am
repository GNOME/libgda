lib_LTLIBRARIES = libgda-client.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

BUILT_SOURCES = gda-stubs.c gda-common.c gda.h
CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/idl/GDA.idl \
	$(top_builddir)/idl/GDA_Command.idl \
	$(top_builddir)/idl/GDA_Connection.idl \
	$(top_builddir)/idl/GDA_Error.idl \
	$(top_builddir)/idl/GDA_Fieldx.idl \
	$(top_builddir)/idl/GDA_Parameter.idl \
	$(top_builddir)/idl/GDA_Recordset.idl

GDA-stubs.c GDA-common.c GDA.h: $(IDLFILES)
	$(ORBIT_IDL) -I$(top_sourcedir)/idl -I`oaf-config --prefix`/share/idl $(top_builddir)/idl/GDA.idl

gda_client_headers = \
	gda-batch.h \
	gda-command.h \
	gda-commandP.h \
	gda-connection-pool.h \
	gda-connection.h \
	gda-error.h \
	gda-field.h \
	gda-recordset.h \
	gda-value.h \
	gda-client.h \
	GDA.h

libgda_client_la_SOURCES = \
	$(gda_client_headers) \
	GDA-stubs.c \
	GDA-common.c \
	gda-batch.c \
	gda-command.c \
	gda-connection-pool.c \
	gda-connection.c \
	gda-error.c \
	gda-field.c \
	gda-recordset.c 
libgda_client_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_client_headers)

