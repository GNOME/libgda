bin_PROGRAMS = gda-mail-srv
noinst_LIBRARIES = libgda-mail.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(CAMEL_CFLAGS)

gda_mail_srv_SOURCES = main-mail.c
gda_mail_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-mail-server/libgda-mail.a

libgda_mail_a_SOURCES = \
	gda-mail-command.c \
	gda-mail-connection.c \
	gda-mail-recordset.c \
	gda-mail.h

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Mail.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
