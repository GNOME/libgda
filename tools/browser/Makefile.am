bin_PROGRAMS=gda-browser-6.0
noinst_LTLIBRARIES = libbrowser.la

SUBDIRS = data common schema-browser query-exec data-manager dummy-perspective

if LDAP
ldap_flags=-DHAVE_LDAP
SUBDIRS+=ldap-browser
LDAP_LDADD=$(top_builddir)/tools/browser/ldap-browser/libperspective.la
endif

if HAVE_GOOCANVAS
SUBDIRS+=canvas
noinst_PROGRAMS=canvas-example
endif
SUBDIRS+= . doc
if HAVE_GDU
SUBDIRS+=help
endif

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        -I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
        -I$(top_builddir) \
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
	$(GLIB_GENMARSHAL) $< --header --prefix=_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_marshal > $@

libbrowser_la_SOURCES=\
	../config-info.h \
	../config-info.c \
	../tool-utils.h \
	../tool-utils.c \
	gdaui-bar.c \
	gdaui-bar.h \
	marshal.c \
	marshal.h \
	decl.h \
	dnd.h \
	dnd.c \
	auth-dialog.c \
	auth-dialog.h \
	login-dialog.c \
	login-dialog.h \
	support.c \
	support.h \
	browser-core.c \
	browser-core.h \
	browser-page.c \
	browser-page.h \
	browser-perspective.c \
	browser-perspective.h \
	browser-spinner.c \
	browser-spinner.h \
	browser-connection.c \
	browser-connection.h \
	browser-connection-priv.h \
	browser-variable.c \
	browser-variable.h \
	browser-virtual-connection.c \
	browser-virtual-connection.h \
	browser-window.c \
	browser-window.h \
	browser-connections-list.c \
	browser-connections-list.h \
	connection-binding-properties.c \
	connection-binding-properties.h \
	mgr-favorites.h \
	mgr-favorites.c \
	browser-stock-icons.c \
	browser-stock-icons.h \
	text-search.c \
	text-search.h

gda_browser_6_0_SOURCES=\
	main.c

$(OBJECTS): marshal.c marshal.h

if PLATFORM_WIN32
EXTRALDFLAGS=-mwindows
gda_browser_6_0_SOURCES += browser-res.rc
else
EXTRALDFLAGS=
endif

if HAVE_GOOCANVAS
CANVAS_LDADD=$(top_builddir)/tools/browser/canvas/libcanvas.la -lm
endif

.rc.o:
	$(WINDRES) $^ -o $@

browser-res.o: browser-res.rc
	$(WINDRES) $^ -o $@

gda_browser_6_0_LDFLAGS = $(EXTRALDFLAGS)
gda_browser_6_0_LDADD=\
	schema-browser/libperspective.la \
	query-exec/libperspective.la \
	data-manager/libperspective.la \
	$(LDAP_LDADD) \
	libbrowser.la \
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	$(CANVAS_LDADD) \
	common/libcommon.la \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-6.0.la \
	$(COREDEPS_LIBS) $(GTK_LIBS) $(GTKSOURCEVIEW_LIBS) $(MAC_INTEGRATION_LIBS)


@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
Desktop_in_files = gda-browser-6.0.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

# canvas example
canvas_example_DEPENDENCIES = \
	libbrowser.la \
	canvas/libcanvas.la \
	common/libcommon.la

canvas_example_SOURCES = \
	../tool-utils.h \
	../tool-utils.c \
	canvas-example.c \
	dnd.c \
	dnd.h

canvas_example_LDFLAGS = $(EXTRALDFLAGS)
canvas_example_CFLAGS = -DCANVAS_EXAMPLE
canvas_example_LDADD=\
	$(CANVAS_LDADD) \
	libbrowser.la \
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	common/libcommon.la \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-6.0.la \
	$(COREDEPS_LIBS) $(GTK_LIBS)

# icons
iconsdir=$(datadir)/libgda-6.0/pixmaps
icons_DATA= \
	gda-browser.png \
	gda-browser-auth.png \
	gda-browser-auth-big.png \
	gda-browser-connected.png \
	gda-browser-connected-big.png \
	gda-browser-non-connected.png \
	gda-browser-non-connected-big.png \
	gda-browser-bookmark.png \
	gda-browser-schema.png \
	gda-browser-table.png \
	gda-browser-column.png \
	gda-browser-column-fknn.png \
	gda-browser-column-fk.png \
	gda-browser-column-nn.png \
	gda-browser-column-pk.png \
	gda-browser-reference.png \
	gda-browser-diagram.png \
	gda-browser-query.png \
	gda-browser-action.png \
	gda-browser-menu-ind.png \
	gda-browser-ldap-entry.png \
	gda-browser-ldap-group.png \
	gda-browser-ldap-organization.png \
	gda-browser-ldap-person.png \
	gda-browser-ldap-class-a.png \
	gda-browser-ldap-class-s.png \
	gda-browser-ldap-class-x.png \
	gda-browser-ldap-class-u.png

# app icon
appiconsdir=$(datadir)/pixmaps
appicons_DATA= \
        gda-browser-6.0.png

EXTRA_DIST = \
	marshal.list \
	browser-res.rc \
	gda-browser.ico \
	$(Desktop_in_files) $(icons_DATA) $(appicons_DATA)

DISTCLEANFILES = $(desktop_DATA)

CLEANFILES = \
	marshal.h \
	marshal.c
