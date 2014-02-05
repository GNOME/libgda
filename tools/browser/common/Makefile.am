noinst_LTLIBRARIES = libcommon.la

if LDAP
ldap_flags=-DHAVE_LDAP
endif

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS) \
	$(ldap_flags)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_common_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_common_marshal > $@

libcommon_la_SOURCES = \
	marshal.c \
	marshal.h \
	objects-cloud.c \
	objects-cloud.h \
	gdaui-data-import.c \
	gdaui-data-import.h \
	gdaui-entry-import.c \
	gdaui-entry-import.h \
	ui-formgrid.c \
	ui-formgrid.h \
	fk-declare.c \
	fk-declare.h \
	widget-overlay.c \
	widget-overlay.h \
	favorites-actions.c \
	favorites-actions.h \
	../../tools-favorites.h \
	../../tools-favorites.c

$(OBJECTS): marshal.c marshal.h

libcommon_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) \
	$(GTK_LIBS)

xmldir=$(datadir)/libgda-6.0
xml_DATA= \
	import_encodings.xml

EXTRA_DIST = \
	marshal.list \
	$(xml_DATA)

CLEANFILES = \
	marshal.h \
	marshal.c
