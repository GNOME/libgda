lib_LTLIBRARIES = libgda-server-2.la

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common

gda_server_headers = \
	gda-server-connection.h \
	gda-server-command.h \
	gda-server-error.h \
	gda-server-field.h \
	gda-server-recordset.h \
	gda-server.h 

libgda_server_2_la_SOURCES = \
	$(gda_server_headers) \
	gda-server-private.h \
	gda-server-command.c \
	gda-server-connection.c \
	gda-server-error.c \
	gda-server-field.c \
	gda-server-init.c \
	gda-server-recordset.c \
	gda-builtin-res.c \
	gda-builtin-res.h \
	gda-server.c 
libgda_server_2_la_LIBADD = -lpopt

gdaincludedir=$(includedir)/gnome/2/gda
gdainclude_HEADERS=$(gda_server_headers)

