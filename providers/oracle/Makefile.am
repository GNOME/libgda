bin_PROGRAMS = gda-oracle-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS)

gda_oracle_srv_2_SOURCES = \
	gda-oracle-connection.c \
	gda-oracle-connection.h \
	gda-oracle-provider.c \
	gda-oracle-provider.h \
	main.c
gda_oracle_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(ORACLE_LIBS)

