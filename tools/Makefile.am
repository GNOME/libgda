if LDAP
ldap_flags=-DHAVE_LDAP
ldap_lib=browser/ldap-browser/libperspective.la
endif

SUBDIRS = misc common base help

bin_PROGRAMS = \
	gda-sql-6.0

if HAVE_UI
bin_PROGRAMS+=gda-browser-6.0
SUBDIRS+=browser
noinst_LTLIBRARIES = libbrowser.la
endif

#TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
TESTS = test-splitter
check_PROGRAMS = $(TESTS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/base \
	-I$(top_srcdir)/tools/common \
	$(ldap_flags) \
	$(COREDEPS_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

gda_sql_6_0_SOURCES = \
        tools-main.c

gda_sql_6_0_LDADD = \
	common/libcommon.la \
	base/libbasetool.la \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

if LIBSOUP
gda_sql_6_0_LDADD += $(LIBSOUP_LIBS)
endif

if HAVE_UI
gda_browser_6_0_SOURCES = \
	tools-main.c \
	tools.gresources.c

if PLATFORM_WIN32
EXTRALDFLAGS=-mwindows
gda_browser_6_0_SOURCES += gda-browser-res.rc
else
EXTRALDFLAGS=
endif

if HAVE_GOOCANVAS
CANVAS_LDADD=browser/canvas/libcanvas.la -lm
endif

libbrowser_la_SOURCES=

libbrowser_la_LIBADD = \
	common/libcommonui.la \
	base/libbasetool.la \
	browser/libbrowsercore.la \
	$(ldap_lib) \
	browser/schema-browser/libperspective.la \
	$(CANVAS_LDADD) \
	browser/dummy-perspective/libperspective.la \
	browser/query-exec/libperspective.la \
	browser/data-manager/libperspective.la

gda_browser_6_0_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(GTK_CFLAGS) \
        $(GTKSOURCEVIEW_CFLAGS) \
        $(MAC_INTEGRATION_CFLAGS) \
	-DIS_BROWSER

gda_browser_6_0_LDFLAGS = $(EXTRALDFLAGS)

gda_browser_6_0_LDADD = \
	libbrowser.la \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB) \
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	$(top_builddir)/libgda-ui/libgda-ui-6.0.la \
	$(GTK_LIBS) $(GTKSOURCEVIEW_LIBS) $(MAC_INTEGRATION_LIBS)

if LIBSOUP
gda_browser_6_0_LDADD += $(LIBSOUP_LIBS)
endif
endif

# resources
RESOURCES=\
	tools.gresource.xml \
	resources/gda-browser-action.png \
	resources/gda-browser-auth-big.png \
	resources/gda-browser-auth.png \
	resources/gda-browser-bookmark.png \
	resources/gda-browser-column-fknn.png \
	resources/gda-browser-column-fk.png \
	resources/gda-browser-column-nn.png \
	resources/gda-browser-column-pk.png \
	resources/gda-browser-column.png \
	resources/gda-browser-connected-big.png \
	resources/gda-browser-connected.png \
	resources/gda-browser-diagram.png \
	resources/gda-browser-ldap-class-a.png \
	resources/gda-browser-ldap-class-s.png \
	resources/gda-browser-ldap-class-u.png \
	resources/gda-browser-ldap-class-x.png \
	resources/gda-browser-ldap-entry.png \
	resources/gda-browser-ldap-group.png \
	resources/gda-browser-ldap-organization.png \
	resources/gda-browser-ldap-person.png \
	resources/gda-browser-menu-ind.png \
	resources/gda-browser-non-connected-big.png \
	resources/gda-browser-non-connected.png \
	resources/gda-browser.png \
	resources/gda-browser-query.png \
	resources/gda-browser-reference.png \
	resources/gda-browser-schema.png \
	resources/gda-browser-table.png \
	resources/gda-browser-view.png

tools.gresources.c: $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=resources --generate-source $(srcdir)/tools.gresource.xml

# setenv() is in libiberty in mingw
if PLATFORM_WIN32

gda_sql_6_0_SOURCES += gda-sql-res.rc
endif

.rc.o:
	$(WINDRES) $^ -o $@

gda-sql-res.o: gda-sql-res.rc
	$(WINDRES) $^ -o $@

gda-browser-res.o: gda-browser-res.rc
	$(WINDRES) $^ -o $@

webdatadir = $(datadir)/libgda-6.0/web
webdata_DATA = \
	webdata/cnc.js \
	webdata/md5.js \
	webdata/jquery.js \
	webdata/mouseapp_2.js \
	webdata/mouseirb_2.js \
	webdata/irb.js \
	webdata/gda.css \
	webdata/gda-print.css \
	webdata/irb.css

EXTRA_DIST = \
	gda-sql-res.rc \
	gda-sql.ico \
	gda-browser-res.rc \
	gda-browser.ico \
	$(webdata_DATA) \
	$(RESOURCES)

mans = \
	gda-sql-6.0.1

man_MANS = \
	$(mans)

install-data-hook:
if DEFAULT_BINARY
	@list='$(mans)'; \
	for i in $$list; do \
	  s=`echo $$i | sed -e 's/.*\\(.\\)$$/\1/'`; \
	  n=`echo $$i | sed -e 's/-[^-]*$$//'`; \
	  f="$$n.$$s"; \
	  echo "cd $(DESTDIR)$(mandir)/man$$s"; \
	  cd $(DESTDIR)$(mandir)/man$$s; \
	  echo "rm -f $$f"; \
	  rm -f $$f; \
	  echo "ln -s $$i $$f"; \
	  ln -s $$i $$f; \
	done
endif

uninstall-local:
if DEFAULT_BINARY
	@list='$(mans)'; \
	for i in $$list; do \
	  s=`echo $$i | sed -e 's/.*\\(.\\)$$/\1/'`; \
	  n=`echo $$i | sed -e 's/-[^-]*$$//'`; \
	  f="$$n.$$s"; \
	  echo " rm -f $(DESTDIR)$(mandir)/man$$s/$$f"; \
	  rm -f $(DESTDIR)$(mandir)/man$$s/$$f; \
	done
endif

install-exec-hook:
if DEFAULT_BINARY
	cd $(DESTDIR)$(bindir) \
	&& rm -f gda-sql$(EXEEXT) \
	&& $(LN_S) gda-sql-6.0$(EXEEXT) gda-sql$(EXEEXT)
endif

uninstall-local:
if DEFAULT_BINARY
	rm -f $(DESTDIR)$(bindir)/gda-sql$(EXEEXT)
endif

test_splitter_SOURCES = test-splitter.c
test_splitter_LDADD = \
	common/libcommon.la \
	base/libbasetool.la \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

if LIBSOUP
test_splitter_LDADD += $(LIBSOUP_LIBS)
endif
