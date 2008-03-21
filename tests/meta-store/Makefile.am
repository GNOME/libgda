AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

noinst_PROGRAMS = check_meta_store_memory check_meta_store_sqlite check_meta_store_postgresql check_meta_store_mysql

common_sources = common.c common.h

check_meta_store_memory_SOURCES = check_meta_store_memory.c $(common_sources)
check_meta_store_memory_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_meta_store_sqlite_SOURCES = check_meta_store_sqlite.c $(common_sources)
check_meta_store_sqlite_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_meta_store_postgresql_SOURCES = check_meta_store_postgresql.c $(common_sources)
check_meta_store_postgresql_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

check_meta_store_mysql_SOURCES = check_meta_store_mysql.c $(common_sources)
check_meta_store_mysql_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = data_builtin_data_types.csv \
	data_columns.csv \
	data_domain_constraints.csv \
	data_domains_1.csv \
	data_domains.csv \
	data_information_schema_catalog_name.csv \
	data_key_column_usage.csv \
	data_parameters.csv \
	data_referential_constraints.csv \
	data_routines.csv \
	data_schemata_1.csv \
	data_schemata.csv \
	data_table_constraints.csv \
	data_tables.csv \
	data_triggers.csv \
	data_view_column_usage.csv \
	data_views.csv
