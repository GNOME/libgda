AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DCHECK_SQL_FILES=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
check_PROGRAMS = check_sqlite
TESTS = check_sqlite

if BDB 
check_PROGRAMS += check_bdb
TESTS += check_bdb
endif

if FIREBIRD
check_PROGRAMS += check_firebird
TESTS += check_firebird
endif

if MDB
check_PROGRAMS += check_mdb
TESTS += check_mdb
endif

if MYSQL
check_PROGRAMS += check_mysql
TESTS += check_mysql
endif

if ORACLE
check_PROGRAMS += check_oracle
TESTS += check_oracle
endif

if POSTGRES
check_PROGRAMS += check_postgres
TESTS += check_postgres
endif

common_sources = \
	../test-errors.h \
	../test-errors.c \
	prov-test-common.h \
	prov-test-common.c \
	prov-test-util.h \
	prov-test-util.c

check_sqlite_SOURCES = $(common_sources) check_sqlite.c
check_sqlite_CFLAGS = 
check_sqlite_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_bdb_SOURCES = $(common_sources) check_bdb.c
check_bdb_CFLAGS = 
check_bdb_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_firebird_SOURCES = $(common_sources) check_firebird.c
check_firebird_CFLAGS = 
check_firebird_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_mdb_SOURCES = $(common_sources) check_mdb.c
check_mdb_CFLAGS = 
check_mdb_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_mysql_SOURCES = $(common_sources) check_mysql.c
check_mysql_CFLAGS = 
check_mysql_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_oracle_SOURCES = $(common_sources) check_oracle.c
check_oracle_CFLAGS = 
check_oracle_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

check_postgres_SOURCES = $(common_sources) check_postgres.c
check_postgres_CFLAGS = 
check_postgres_LDADD =  \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/tests/libgda-test-6.0.la \
	$(COREDEPS_LIBS)

EXTRA_DIST = \
	README \
	gda_check_db.mdb \
	PostgreSQL_create_tables.sql \
	SQLite_create_tables.sql \
	MySQL_create_tables.sql \
	TYPES_SCHEMA_Mdb.xml \
	TYPES_SCHEMA_MySQL.xml \
	TYPES_SCHEMA_PostgreSQL.xml \
	TYPES_SCHEMA_SQLite.xml 
