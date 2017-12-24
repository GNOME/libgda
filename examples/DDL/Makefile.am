bin_PROGRAMS = ddl

ddl_SOURCES = ddl.c

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda \
	$(COREDEPS_CFLAGS)
#	-I$(top_srcdir)/libgda/sqlite \
#	-I$(top_builddir)/libgda/sqlite \
#	-DABI_VERSION=\""$(GDA_ABI_VERSION)"\" \
#	$(BDB_CFLAGS) \
#	-DBDB_LIB=\""$(BDB_LIB)"\" \
#	$(GIO_CFLAGS) $(LIBSECRET_CFLAGS) $(GNOME_KEYRING_CFLAGS)

AM_LDFLAGS = $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)


