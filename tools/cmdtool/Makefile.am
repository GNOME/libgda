noinst_LTLIBRARIES = libcmdtool.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
        $(COREDEPS_CFLAGS) \
        $(COREDEPS_WFLAGS)


libcmdtool_la_SOURCES = \
	tool.h \
	tool-decl.h \
        tool-errors.h \
        tool-errors.c \
        tool-input.h \
        tool-input.c \
        tool-output.h \
        tool-output.c \
        tool-command.h \
        tool-command.c \
	tool-help.h \
	tool-help.c
