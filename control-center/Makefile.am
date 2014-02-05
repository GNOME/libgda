bin_PROGRAMS=gda-control-center-6.0

SUBDIRS = data

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        -I$(top_builddir) \
        $(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
        $(GTK_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\" \
	-DGDA_ABI_VERSION=\""$(GDA_ABI_VERSION)"\"

# rename gdaui-dsn-editor as dsn-properties-editor
gda_control_center_6_0_SOURCES=\
	gdaui-login-dialog.c \
	gdaui-login-dialog.h \
	gdaui-bar.c \
	gdaui-bar.h \
	dsn-config.c \
	dsn-config.h \
	gdaui-dsn-assistant.c \
	gdaui-dsn-assistant.h \
	gdaui-dsn-editor.c \
	gdaui-dsn-editor.h \
	dsn-properties-dialog.c \
	dsn-properties-dialog.h \
	main.c \
	provider-config.c \
	provider-config.h

if PLATFORM_WIN32
EXTRALDFLAGS=-mwindows
gda_control_center_6_0_SOURCES += control-center-res.rc
else
EXTRALDFLAGS=
endif

.rc.o:
	$(WINDRES) $^ -o $@

control-center-res.o: control-center-res.rc
	$(WINDRES) $^ -o $@

gda_control_center_6_0_LDFLAGS = $(EXTRALDFLAGS)
gda_control_center_6_0_LDADD=\
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-6.0.la \
	$(COREDEPS_LIBS) $(GTK_LIBS)

@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
Desktop_in_files = gda-control-center-6.0.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

# icons
iconsdir=$(datadir)/libgda-6.0/pixmaps
icons_DATA= \
	gda-control-center.png \
	gda-control-center-newcnc.png

EXTRA_DIST = \
	control-center-res.rc \
	gda-control-center.ico \
	$(Desktop_in_files) $(icons_DATA)
DISTCLEANFILES = $(desktop_DATA)
