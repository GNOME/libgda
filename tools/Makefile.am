SUBDIRS = templates

bin_PROGRAMS = gda-run
bin_SCRIPTS = gda-config gda-buildserver gda-buildclient

INCLUDES = \
	-I. \
	$(GDA_CLIENT_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client

gda_run_SOURCES = gda-run.c
gda_run_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la

EXTRA_DIST = \
	gda-config.in \
	gda-buildserver.in \
	gda-buildclient.in
