noinst_LTLIBRARIES = libgda-postgres.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) $(POSTGRES_CFLAGS) 

keyword_files=keywords_82.list keywords_83.list keywords_84.list
pkeyword_files=$(addprefix $(top_srcdir)/providers/reuseable/postgres/,$(keyword_files))

mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -g -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(pkeyword_files)
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/postgres/keywords_82.list V82 > keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/postgres/keywords_83.list V83 >> keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/reuseable/postgres/keywords_84.list V84 >> keywords_hash.c

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d $(srcdir)/parser.y $(top_srcdir)/libgda/sql-parser/lempar.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_builddir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

postgres_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > postgres_token_types.h

$(OBJECTS) $(libgda_postgres_la_OBJECTS): postgres_token_types.h keywords_hash.c

libgda_postgres_la_SOURCES = \
	gda-postgres-reuseable.c \
	gda-postgres-reuseable.h \
	gda-postgres-parser.c \
	gda-postgres-parser.h \
	gda-postgres-meta.c \
	gda-postgres-meta.h \
	parser.h \
        parser.c \
        postgres_token_types.h

libgda_postgres_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) $(POSTGRES_LIBS)

EXTRA_DIST = parser.y gen_def.c $(keyword_files)
CLEANFILES = parser.h parser.c parser.out postgres_token_types.h gen_def$(EXEEXT_FOR_BUILD) \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c
