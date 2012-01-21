if HAVE_UI
BROWSER_DIR=browser
endif

SUBDIRS = $(BROWSER_DIR) help

bin_PROGRAMS = \
	gda-list-config-5.0 \
	gda-sql-5.0 \
	gda-list-server-op-5.0

noinst_PROGRAMS = information-schema-doc information-schema-types

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I. \
	$(COREDEPS_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

gda_list_config_5_0_SOURCES = \
        gda-list-config.c

gda_list_config_5_0_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(COREDEPS_LIBS)

gda_sql_5_0_SOURCES = \
	config-info.h \
	config-info.c \
	tools-favorites.h \
	tools-favorites.c \
	tools-utils.h \
	tools-utils.c \
        gda-sql.c \
	gda-sql.h \
	gda-threader.h \
	gda-threader.c \
	tools-input.h \
	tools-input.c \
	command-exec.h \
	command-exec.c \
	$(top_srcdir)/libgda/global.h \
	$(top_srcdir)/libgda/md5.h \
	$(top_srcdir)/libgda/md5c.c

gda_sql_5_0_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(COREDEPS_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

if LIBSOUP
gda_sql_5_0_SOURCES += web-server.h web-server.c html-doc.h html-doc.c
AM_CPPFLAGS += $(LIBSOUP_CFLAGS)
gda_sql_5_0_LDADD += $(LIBSOUP_LIBS)
endif



# setenv() is in libiberty in mingw
if PLATFORM_WIN32
gda_sql_5_0_LDADD += -liberty
gda_sql_5_0_SOURCES += gda-sql-res.rc
endif

.rc.o:
	$(WINDRES) $^ -o $@

gda-sql-res.o: gda-sql-res.rc
	$(WINDRES) $^ -o $@

gda_list_server_op_5_0_SOURCES = \
        gda-list-server-op.c \
	gda-tree-mgr-xml.c \
	gda-tree-mgr-xml.h

gda_list_server_op_5_0_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(COREDEPS_LIBS)


information_schema_doc_SOURCES = \
        information-schema-doc.c

information_schema_doc_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(COREDEPS_LIBS)

information_schema_types_SOURCES = \
        information-schema-types.c

information_schema_types_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(COREDEPS_LIBS)

webdatadir = $(datadir)/libgda-5.0/web
webdata_DATA = \
	cnc.js \
	md5.js \
	jquery.js \
	mouseapp_2.js \
	mouseirb_2.js \
	irb.js \
	gda.css \
	gda-print.css \
	irb.css

EXTRA_DIST = \
	gda-sql-res.rc \
	gda-sql.ico \
	$(webdata_DATA)

mans = \
	gda-sql-5.0.1

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
	&& rm -f gda-list-config$(EXEEXT) gda-sql$(EXEEXT) gda-list-server-op$(EXEEXT) \
	&& $(LN_S) gda-list-config-5.0$(EXEEXT) gda-list-config$(EXEEXT) \
	&& $(LN_S) gda-sql-5.0$(EXEEXT) gda-sql$(EXEEXT) \
	&& $(LN_S) gda-list-server-op-5.0$(EXEEXT) gda-list-server-op$(EXEEXT)
endif

uninstall-local:
if DEFAULT_BINARY
	rm -f $(DESTDIR)$(bindir)/gda-list-config$(EXEEXT) $(DESTDIR)$(bindir)/gda-sql$(EXEEXT) $(DESTDIR)$(bindir)/gda-list-server-op$(EXEEXT)
endif
