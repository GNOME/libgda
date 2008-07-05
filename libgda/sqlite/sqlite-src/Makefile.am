noinst_LTLIBRARIES = \
	libsqlite.la
AM_CPPFLAGS = -DSQLITE_ENABLE_COLUMN_METADATA -DTHREADSAFE=1

# Download the Windows prepared SQLite (series 3.x) sources in a ZIP, and
# extract the zip in this directory (thanks to http://aaronbock.net/articles/sqlite!)
#
# files list generated with: ls *.c *.h | sort | sed -e 's/$/ \\/' -e 's/^/\t/'
# and remove shell.c and tclsqlite.c
# 
# As of version 3.3.14, SQLite now ships only two files: sqlite3.c (an aggregate of all the source files) and sqlite3.h.
# Apply the 'PragmasPatch' patch to sqlite3.c
libsqlite_la_SOURCES = \
	sqlite3.c \
	sqlite3.h 

EXTRA_DIST = PragmasPatch
