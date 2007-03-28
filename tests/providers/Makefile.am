AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DCHECK_SQL_FILES=\""$(top_srcdir)"\"

check_PROGRAMS = check_sqlite
TESTS = check_sqlite

if BDB 
check_PROGRAMS += check_bdb
TESTS += check_bdb
endif

if IBMDB2
check_PROGRAMS += check_ibmdb2
TESTS += check_ibmdb2
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

if MSQL
check_PROGRAMS += check_msql
TESTS += check_msql
endif

if ODBC
check_PROGRAMS += check_odbc
TESTS += check_odbc
endif

if ORACLE
check_PROGRAMS += check_oracle
TESTS += check_oracle
endif

if POSTGRES
check_PROGRAMS += check_postgres
TESTS += check_postgres
endif

if SYBASE
check_PROGRAMS += check_sybase
TESTS += check_sybase
endif

if FREETDS
check_PROGRAMS += check_freetds
TESTS += check_freetds
endif

if XBASE
check_PROGRAMS += check_xbase
TESTS += check_xbase
endif

if LDAP
check_PROGRAMS += check_ldap
TESTS += check_ldap
endif

common_sources = \
	prov-test-common.h \
	prov-test-common.c \
	prov-test-util.h \
	prov-test-util.c

check_sqlite_SOURCES = $(common_sources) check_sqlite.c
check_sqlite_CFLAGS = @CHECK_CFLAGS@
check_sqlite_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_bdb_SOURCES = $(common_sources) check_bdb.c
check_bdb_CFLAGS = @CHECK_CFLAGS@
check_bdb_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_ibmdb2_SOURCES = $(common_sources) check_ibmdb2.c
check_ibmdb2_CFLAGS = @CHECK_CFLAGS@
check_ibmdb2_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_firebird_SOURCES = $(common_sources) check_firebird.c
check_firebird_CFLAGS = @CHECK_CFLAGS@
check_firebird_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_mdb_SOURCES = $(common_sources) check_mdb.c
check_mdb_CFLAGS = @CHECK_CFLAGS@
check_mdb_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_mysql_SOURCES = $(common_sources) check_mysql.c
check_mysql_CFLAGS = @CHECK_CFLAGS@
check_mysql_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_msql_SOURCES = $(common_sources) check_msql.c
check_msql_CFLAGS = @CHECK_CFLAGS@
check_msql_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_odbc_SOURCES = $(common_sources) check_odbc.c
check_odbc_CFLAGS = @CHECK_CFLAGS@
check_odbc_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_oracle_SOURCES = $(common_sources) check_oracle.c
check_oracle_CFLAGS = @CHECK_CFLAGS@
check_oracle_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_postgres_SOURCES = $(common_sources) check_postgres.c
check_postgres_CFLAGS = @CHECK_CFLAGS@
check_postgres_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_sybase_SOURCES = $(common_sources) check_sybase.c
check_sybase_CFLAGS = @CHECK_CFLAGS@
check_sybase_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_freetds_SOURCES = $(common_sources) check_freetds.c
check_freetds_CFLAGS = @CHECK_CFLAGS@
check_freetds_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_xbase_SOURCES = $(common_sources) check_xbase.c
check_xbase_CFLAGS = @CHECK_CFLAGS@
check_xbase_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_ldap_SOURCES = $(common_sources) check_ldap.c
check_ldap_CFLAGS = @CHECK_CFLAGS@
check_ldap_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = \
	README \
	PostgreSQL_create_tables.sql \
	SQLite_create_tables.sql
