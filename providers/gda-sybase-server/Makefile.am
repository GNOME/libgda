bin_PROGRAMS = gda-sybase-srv
noinst_LIBRARIES = libgda-sybase.a

INCLUDES = \
	$(SYBASE_CFLAGS) \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server 

gda_sybase_srv_SOURCES = main-sybase.c
gda_sybase_srv_LDADD = \
	$(SYBASE_LIBS) \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-sybase-server/libgda-sybase.a 

libgda_sybase_a_SOURCES = \
	gda-sybase-command.c \
	gda-sybase-command.h \
	gda-sybase-connection.c \
	gda-sybase-connection.h \
	gda-sybase-error.c \
	gda-sybase-error.h \
	gda-sybase-recordset.c \
	gda-sybase-recordset.h \
	gda-sybase-types.c \
	gda-sybase-types.h \
	gda-sybase.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Sybase.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
