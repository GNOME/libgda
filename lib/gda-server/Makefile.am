lib_LTLIBRARIES = libgda-server.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common

BUILT_SOURCES = gda-skels.c gda-common.c gda.h
IDL_FILES = \
	$(top_builddir)/idl/gda.idl \
	$(top_builddir)/idl/gda-command.idl \
	$(top_builddir)/idl/gda-connection.idl \
	$(top_builddir)/idl/gda-error.idl \
	$(top_builddir)/idl/gda-fieldx.idl \
	$(top_builddir)/idl/gda-parameter.idl \
	$(top_builddir)/idl/gda-recordset.idl

gda-skels.c gda-common.c gda.h: $(IDLFILES)
	$(ORBIT_IDL) -I$(top_sourcedir)/idl -I`oaf-config --prefix`/share/idl $(top_builddir)/idl/gda.idl

gda_server_headers = \
	gda-server-impl.h \
	gda-server.h \
	gda.h

libgda_server_la_SOURCES = \
	gda-common.c \
	gda-skels.c \
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
libgda_server_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_server_headers)
