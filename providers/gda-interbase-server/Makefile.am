bin_PROGRAMS = gda-interbase-srv
noinst_LIBRARIES = libgda-interbase.a

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_srcdir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_srcdir)/lib/gda-server \
	-I$(top_builddir)/lib/gda-server \
	$(INTERBASE_CFLAGS)

gda_interbase_srv_SOURCES = main-interbase.c
gda_interbase_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-interbase-server/libgda-interbase.a \
	$(INTERBASE_LIBS)

libgda_interbase_a_SOURCES = \
	gda-interbase-command.c \
	gda-interbase-connection.c \
	gda-interbase-recordset.c \
	gda-interbase.h

oafdir = $(DESTDIR)$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_InterBase.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
