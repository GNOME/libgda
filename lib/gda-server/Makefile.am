lib_LTLIBRARIES = libgda-server.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_server_headers = \
	gda-server-impl.h \
	gda-server.h

libgda_server_la_SOURCES = \
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
