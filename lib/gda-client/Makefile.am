lib_LTLIBRARIES = libgda-client.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_client_headers = \
	gda-batch.h \
	gda-command.h \
	gda-commandP.h \
	gda-connection-pool.h \
	gda-connection.h \
	gda-error.h \
	gda-field.h \
	gda-recordset.h \
	gda-report.h \
	gda-value.h \
	gda-client.h

libgda_client_la_SOURCES = \
	$(gda_client_headers)
	gda-batch.c \
	gda-command.c \
	gda-connection-pool.c \
	gda-connection.c \
	gda-error.c \
	gda-field.c \
	gda-recordset.c \
	gda-report.c
libgda_client_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_client_headers)
