bin_PROGRAMS = \
	gda-list-config-6.0 \
	gda-list-server-op-6.0


noinst_PROGRAMS = information-schema-doc information-schema-types

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir)/libgda \
	$(ldap_flags) \
	$(COREDEPS_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

gda_list_config_6_0_SOURCES = \
        gda-list-config.c

gda_list_config_6_0_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

gda_list_server_op_6_0_SOURCES = \
        gda-list-server-op.c \
	gda-tree-mgr-xml.c \
	gda-tree-mgr-xml.h

gda_list_server_op_6_0_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

information_schema_doc_SOURCES = \
        information-schema-doc.c

information_schema_doc_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)

information_schema_types_SOURCES = \
        information-schema-types.c

information_schema_types_LDADD = \
        $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)


install-exec-hook:
if DEFAULT_BINARY
	cd $(DESTDIR)$(bindir) \
	&& rm -f gda-list-config$(EXEEXT) gda-list-server-op$(EXEEXT) \
	&& $(LN_S) gda-list-config-6.0$(EXEEXT) gda-list-config$(EXEEXT) \
	&& $(LN_S) gda-list-server-op-6.0$(EXEEXT) gda-list-server-op$(EXEEXT)
endif

uninstall-local:
if DEFAULT_BINARY
	rm -f $(DESTDIR)$(bindir)/gda-list-config$(EXEEXT) $(DESTDIR)$(bindir)/gda-list-server-op$(EXEEXT)
endif
