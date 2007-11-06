noinst_LTLIBRARIES = libgda_graph-4.0.la

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) 

libgda_graph_headers = \
	gda-graph.h \
	gda-graph-item.h \
	gda-graph-query.h \
	gda-dict-reg-graphs.h

libgda_graphincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/graph
libgda_graphinclude_HEADERS=$(libgda_graph_headers)

libgda_graph_4_0_la_SOURCES = \
	$(libgda_graph_headers) \
	gda-graph.c \
        gda-graph-item.c \
        gda-graph-query.c \
	gda-dict-reg-graphs.c
