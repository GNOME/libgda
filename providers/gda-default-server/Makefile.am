SUBDIRS = sqlite

bin_PROGRAMS = gda-default-srv
noinst_LIBRARIES = libgda-default.a

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_srcdir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_srcdir)/lib/gda-server \
	-I$(top_builddir)/lib/gda-server \
	-I$(top_srcdir)/providers/gda-default-server \
	-I$(top_builddir)/providers/gda-default-server \
	-I$(top_srcdir)/providers/gda-default-server/sqlite \
	-I$(top_builddir)/providers/gda-default-server/sqlite

gda_default_srv_SOURCES = main-default.c
gda_default_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-default-server/libgda-default.a \
	$(top_builddir)/providers/gda-default-server/sqlite/libsqlite.a \
	-lgdbm

libgda_default_a_SOURCES = \
	gda-default-command.c \
	gda-default-connection.c \
	gda-default-recordset.c \
	gda-default.h

oafdir=$(DESTDIR)$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Default.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=\
	$(oaf_in_files) \
	$(oaf_DATA)
