lib_LTLIBRARIES = libgda-client-2.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

gda_client_headers = \
	gda-batch.h \
	gda-command.h \
	gda-connection-pool.h \
	gda-connection.h \
	gda-export.h \
	gda-field.h \
	gda-recordset.h \
	gda-value.h \
	gda-client.h 

libgda_client_2_la_SOURCES = \
	$(gda_client_headers) \
	gda-batch.c \
	gda-command.c \
	gda-connection-pool.c \
	gda-connection.c \
	gda-export.c \
	gda-field.c \
	gda-recordset.c 
libgda_client_2_la_LIBADD =

gdaincludedir=$(includedir)/gnome/2/gda
gdainclude_HEADERS=$(gda_client_headers)

