bin_PROGRAMS = gda-mail-srv
lib_LTLIBRARIES = libgda-mail.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(CAMEL_CFLAGS)

gda_mail_srv_SOURCES = main-mail.c
gda_mail_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-mail-server/libgda-mail.la

libgda_mail_la_SOURCES = \
	gda-mail-command.c \
	gda-mail-connection.c \
	gda-mail-recordset.c \
	gda-mail.h

EXTRA_DIST = gda-mail.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=GNOME_GDA_Provider_Mail.oafinfo
