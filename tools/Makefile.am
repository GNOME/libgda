SUBDIRS = binreloc

bin_PROGRAMS = \
	gda-list-config-4.1 \
	gda-sql-4.1 \
	gda-list-server-op-4.1

noinst_PROGRAMS = information-schema-doc

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I. \
	$(LIBGDA_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

gda_list_config_4_1_SOURCES = \
        gda-list-config.c

gda_list_config_4_1_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_sql_4_1_SOURCES = \
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

gda_sql_4_1_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
	binreloc/libgdasql_binreloc-4.0.la \
        $(LIBGDA_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

if LIBSOUP
gda_sql_4_1_SOURCES += web-server.h web-server.c html-doc.h html-doc.c
AM_CPPFLAGS += $(LIBSOUP_CFLAGS)
gda_sql_4_1_LDADD += $(LIBSOUP_LIBS)
endif



# setenv() is in libiberty in mingw
if PLATFORM_WIN32
gda_sql_4_1_LDADD += -liberty
gda_sql_4_1_SOURCES += gda-sql-res.rc
endif

.rc.o:
	$(WINDRES) $^ -o $@

gda-sql-res.o: gda-sql-res.rc
	$(WINDRES) $^ -o $@

gda_list_server_op_4_1_SOURCES = \
        gda-list-server-op.c

gda_list_server_op_4_1_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)


information_schema_doc_SOURCES = \
        information-schema-doc.c

information_schema_doc_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

webdatadir = $(datadir)/libgda-4.1/web
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

EXTRA_DIST = gda-sql.ico \
	$(webdata_DATA)

mans = \
	gda-sql-4.1.1

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
	&& $(LN_S) gda-list-config-4.1$(EXEEXT) gda-list-config$(EXEEXT) \
	&& $(LN_S) gda-sql-4.1$(EXEEXT) gda-sql$(EXEEXT) \
	&& $(LN_S) gda-list-server-op-4.1$(EXEEXT) gda-list-server-op$(EXEEXT)
endif

uninstall-local:
if DEFAULT_BINARY
	rm -f $(DESTDIR)$(bindir)/gda-list-config$(EXEEXT) $(DESTDIR)$(bindir)/gda-sql$(EXEEXT) $(DESTDIR)$(bindir)/gda-list-server-op$(EXEEXT)
endif
