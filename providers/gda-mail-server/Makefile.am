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
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-mail-server/libgda-mail.a

libgda_mail_a_SOURCES = \
	gda-mail-command.c \
	gda-mail-connection.c \
	gda-mail-recordset.c \
	gda-mail.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Mail.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
