noinst_LTLIBRARIES = libgda-mysql.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

keyword_files=keywords_V50.list keywords_V51.list keywords_V54.list keywords_V60.list
pkeyword_files=$(addprefix $(top_srcdir)/providers/reuseable/mysql/,$(keyword_files))

mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(pkeyword_files)
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/mysql/keywords_V50.list V50 > keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/mysql/keywords_V51.list V51 >> keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/mysql/keywords_V54.list V54 >> keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/mysql/keywords_V60.list V60 >> keywords_hash.c

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d $(srcdir)/parser.y $(top_srcdir)/libgda/sql-parser/lempar.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_builddir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

mysql_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > mysql_token_types.h

$(OBJECTS) $(libgda_mysql_la_OBJECTS): mysql_token_types.h keywords_hash.c

libgda_mysql_la_SOURCES = \
	gda-mysql-reuseable.c \
	gda-mysql-reuseable.h \
	gda-mysql-parser.c \
	gda-mysql-parser.h \
	gda-mysql-meta.c \
	gda-mysql-meta.h \
	parser.h \
        parser.c \
        mysql_token_types.h

libgda_mysql_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = $(xml_in_files) parser.y gen_def.c $(keyword_files)

CLEANFILES = parser.h parser.c parser.out mysql_token_types.h gen_def$(EXEEXT_FOR_BUILD) \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c
