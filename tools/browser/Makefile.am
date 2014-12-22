noinst_LTLIBRARIES = libbrowsercore.la

SUBDIRS = dummy-perspective schema-browser query-exec data-manager
if LDAP
SUBDIRS += ldap-browser
endif
if HAVE_GOOCANVAS
SUBDIRS += canvas
endif
SUBDIRS += help

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        -I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
        -I$(top_builddir) \
        -I$(top_builddir)/libgda \
        -I$(srcdir)/.. \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/base \
	-I$(top_srcdir)/tools/common \
        $(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
        $(GTK_CFLAGS) \
        $(GTKSOURCEVIEW_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\" \
	$(ldap_flags)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_common_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_common_marshal > $@

libbrowsercore_la_SOURCES=\
	marshal.c \
	marshal.h \
	decl.h \
	dnd.h \
	dnd.c \
	ui-support.h \
	ui-support.c \
	auth-dialog.h \
	auth-dialog.c \
	login-dialog.h \
	login-dialog.c \
	browser-perspective.h \
	browser-perspective.c \
	browser-page.h \
	browser-page.c \
	browser-window.h \
	browser-window.c \
	browser-connections-list.h \
	browser-connections-list.c \
	connection-binding-properties.h \
	connection-binding-properties.c \
	gdaui-data-import.h \
	gdaui-data-import.c \
	gdaui-entry-import.h \
	gdaui-entry-import.c \
	objects-cloud.h \
	objects-cloud.c \
	fk-declare.h \
	fk-declare.c \
	gdaui-bar.h \
	gdaui-bar.c \
	mgr-favorites.h \
	mgr-favorites.c \
	widget-overlay.h \
	widget-overlay.c \
	ui-formgrid.h \
	ui-formgrid.c \
	text-search.h \
	text-search.c

$(OBJECTS): marshal.c marshal.h

# app icon
appiconsdir=$(datadir)/pixmaps
appicons_DATA= \
	data/256x256/gda-browser-6.0.png

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = gda-browser-6.0.appdata.xml.in

@INTLTOOL_DESKTOP_RULE@
desktopdir=$(datadir)/applications
Desktop_in_files = gda-browser-6.0.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	marshal.list \
	$(Desktop_in_files) \
	$(appdata_in_files) \
	$(appicons_DATA)

CLEANFILES = \
	$(appdata_DATA)

