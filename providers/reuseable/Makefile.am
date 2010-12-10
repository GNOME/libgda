noinst_LTLIBRARIES = libreuseable.la

SUBDIRS = \
	postgres \
	mysql

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        -I$(top_srcdir)/libgda/sqlite \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS)

libreuseable_la_SOURCES = \
	gda-provider-reuseable.h \
	reuse-all.c \
	reuse-all.h

libreuseable_la_LIBADD = \
	postgres/libgda-postgres.la \
	mysql/libgda-mysql.la
