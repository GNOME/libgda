lib_LTLIBRARIES = libgda-client.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

BUILT_SOURCES = gda-stubs.c gda-common.c gda.h
IDL_FILES = \
	$(top_builddir)/idl/gda.idl \
	$(top_builddir)/idl/gda-command.idl \
	$(top_builddir)/idl/gda-connection.idl \
	$(top_builddir)/idl/gda-error.idl \
	$(top_builddir)/idl/gda-fieldx.idl \
	$(top_builddir)/idl/gda-parameter.idl \
	$(top_builddir)/idl/gda-recordset.idl

gda-stubs.c gda-common.c gda.h: $(IDLFILES)
	$(ORBIT_IDL) -I$(datadir)/idl -I$(top_builddir)/idl `gnome-config --cflags idl` $(top_builddir)/idl/gda.idl

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
	gda.h

libgda_client_la_SOURCES = \
	$(gda_client_headers) \
	gda-stubs.c \
	gda-common.c \
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

