bin_PROGRAMS = gda-default-srv
noinst_LIBRARIES = libgda-default.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_default_srv_SOURCES = main-default.c
gda_default_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-default-server/libgda-default.a \
	$(top_builddir)/providers/gda-default-server/libsqlite.a \
	-lgdbm

libgda_default_a_SOURCES = \
	gda-default-command.c \
	gda-default-connection.c \
	gda-default-recordset.c \
	gda-default.h

build_sqlite:
	mkdir build_sqlite && \
	cd build_sqlite && \
	../sqlite/configure && \
	make

gda-default.h: build_sqlite

$(top_builddir)/providers/gda-default-server/libsqlite.a: build_sqlite
	cp build_sqlite/libsqlite.a libsqlite.a

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Default.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
