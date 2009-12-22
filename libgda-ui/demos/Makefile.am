## Makefile.am for libgda-ui demos

democodedir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/demo

if PLATFORM_WIN32
EXTRALDFLAGS=-mwindows
else
EXTRALDFLAGS=
endif

## These should be in the order you want them to appear in the 
## demo app, which means alphabetized by demo title, not filename
demos =						\
	data_model_dir.c \
	form.c \
	form_rw.c \
	form_pict.c \
	form_data_layout.c \
	grid.c \
	grid_rw.c \
	grid_pict.c \
	grid_data_layout.c \
	linked_grid_form.c \
	linked_model_param.c \
	ddl_queries.c \
	login.c \
	provider_sel.c \
	tree.c \
	cloud.c \
	combo.c

demofiles = demo_db.db example_automatic_layout.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
        -I$(top_builddir) \
	-I$(top_builddir)/libgda \
        $(LIBGDA_CFLAGS) \
        $(GTK_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\"

bin_PROGRAMS = gdaui-demo-4.0

BUILT_SOURCES = demos.h

demos.h: $(demos) geninclude.pl
	(here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

gdaui_demo_4_0_SOURCES = 		\
	$(demos)		\
	demo-common.h		\
	main.c			\
	demos.h

gdaui_demo_4_0_DEPENDENCIES = $(DEPS)
gdaui_demo_4_0_LDFLAGS =  $(EXTRALDFLAGS)
gdaui_demo_4_0_LDADD =  \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-4.0.la \
	$(LIBGDA_LIBS) $(GTK_LIBS)

democode_DATA = $(demos) $(demofiles)

DISTCLEANFILES = demos.h
EXTRA_DIST = $(demofiles)
