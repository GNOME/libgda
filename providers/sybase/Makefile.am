bin_PROGRAMS = gda-sybase-srv

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(SYBASE_CFLAGS)

gda_sybase_srv_SOURCES = \
	gda-sybase-provider.c \
	gda-sybase-provider.h \
	gda-sybase.h \
	main.c

gda_sybase_srv_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(SYBASE_LIBS)

