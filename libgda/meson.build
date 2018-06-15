inc_libgdah = include_directories ('.')
inc_libgdah_dep = declare_dependency (include_directories : inc_libgdah)


# Build auxiliary executables and libraries
subdir('sqlite')
subdir('sql-parser')
subdir('providers-support')
subdir('handlers')
subdir('thread-wrapper')
subdir('binreloc')

conf = configuration_data()
# FIXME: Set this variables
# conf.set_quoted('LIBGDA_BDB_INC','')
# conf.set_quoted('LIBGDA_LDAP_INC','')
# conf.set_quoted('LIBGDA_LDAP_INC2','')
libgda_virtualh = configure_file(
		input: 'libgda.h.in',
		output : 'libgda.h',
		configuration : conf)


gda_marshalls = gnome_module.genmarshal('gda-marshal',prefix : '_gda_marshal',
					sources : 'gda-marshal.list')

gda_enum_headers = files ('gda-connection.h', 'gda-enums.h')

gda_enums = gnome_module.mkenums_simple('gda-enum-types', sources: gda_enum_headers)

mkkeywordhash = executable ('mkkeywordhash', 'sqlite/mkkeywordhash.c')

gda_keywordshash = custom_target('keywords_hash',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords.list',
		output: 'keywords_hash.code')

gda_keywordshash_dep = declare_dependency (sources: gda_keywordshash)

libgda_headers = files([
	'gda-attributes-manager.h',
	'gda-batch.h',
	'gda-blob-op.h',
	'gda-column.h',
	'gda-config.h',
	'gda-connection-event.h',
	'gda-connection-private.h',
	'gda-data-comparator.h',
	'gda-data-handler.h',
	'gda-data-model-array.h',
	'gda-data-model.h',
	'gda-data-model-dir.h',
	'gda-data-model-extra.h',
	'gda-data-model-import.h',
	'gda-data-model-iter.h',
	'gda-data-model-iter-extra.h',
	'gda-data-model-private.h',
	'gda-data-access-wrapper.h',
	'gda-data-proxy.h',
	'gda-data-select.h',
	'gda-decl.h',
	'gda-holder.h',
	'gda-lockable.h',
	'gda-log.h',
	'gda-meta-store.h',
	'gda-meta-struct.h',
	'gda-quark-list.h',
	'gda-repetitive-statement.h',
	'gda-row.h',
	'gda-set.h',
	'gda-server-operation.h',
	'gda-server-provider.h',
	'gda-server-provider-extra.h',
	'gda-server-provider-private.h',
	'gda-server-provider-impl.h',
	'gda-statement.h',
	'gda-statement-extra.h',
	'gda-sql-builder.h',
	'gda-transaction-status.h',
	'gda-transaction-status-private.h',
	'gda-tree.h',
	'gda-tree-mgr-columns.h',
	'gda-tree-mgr-schemas.h',
	'gda-tree-mgr-tables.h',
	'gda-tree-node.h',
	'gda-tree-manager.h',
	'gda-tree-mgr-label.h',
	'gda-tree-mgr-select.h',
	'gda-util.h',
	'gda-value.h',
	'gda-xa-transaction.h',
	'libgda-global-variables.h',
	'gda-data-pivot.h'
])

libgda_headers += gda_enum_headers

libgda_sources= files([
	'gda-batch.c',
	'gda-blob-op.c',
	'gda-blob-op-impl.h',
	'gda-column.c',
	'gda-config.c',
	'gda-connection.c',
	'gda-connection-event.c',
	'gda-data-comparator.c',
	'gda-data-handler.c',
	'gda-data-model-array.c',
	'gda-data-model.c',
	'gda-data-model-dir.c',
	'gda-data-model-import.c',
	'gda-data-model-iter.c',
	'gda-data-access-wrapper.c',
	'gda-data-proxy.c',
	'gda-data-select.c',
	'gda-holder.c',
	'gda-init.c',
	'gda-lockable.c',
	'gda-log.c',
	'gda-meta-store.c',
	'gda-meta-struct.c',
	'gda-meta-struct-io.c',
	'gda-quark-list.c',
	'gda-repetitive-statement.c',
	'gda-row.c',
	'gda-set.c',
	'gda-server-operation.c',
	'gda-server-provider.c',
	'gda-server-provider-extra.c',
	'gda-statement.c',
	'gda-sql-builder.c',
	'gda-transaction-status.c',
	'gda-tree.c',
	'gda-tree-mgr-columns.c',
	'gda-tree-mgr-schemas.c',
	'gda-tree-mgr-tables.c',
	'gda-tree-node.c',
	'gda-tree-manager.c',
	'gda-tree-mgr-label.c',
	'gda-tree-mgr-select.c',
	'gda-util.c',
	'gda-value.c',
	'gda-xa-transaction.c',
	'gda-data-pivot.c'
	])

libgda_source_all = files([
	'csv.h',
	'libcsv.c',
	'dir-blob-op.h',
	'dir-blob-op.c',
	'gda-attributes-manager.c',
	'gda-debug-macros.h',
	'gda-connection-internal.h',
	'gda-connection-sqlite.h',
	'gda-custom-marshal.c',
	'gda-custom-marshal.h',
	'gda-data-meta-wrapper.c',
	'gda-data-meta-wrapper.h',
	'gda-data-model-dsn-list.c',
	'gda-data-model-dsn-list.h',
	'gda-data-select-extra.h',
	'gda-meta-store-extra.h',
	'gda-meta-struct-private.h',
	'gda-server-operation-private.h',
	'gda-types.c',
	'gda-types.h',
	'gda-statement-priv.h',
	])

libgda_source_all += [
	libgda_sources,
	libgda_headers,
	gda_enums,
	gda_marshalls,
	sqlitesources
	]
libgda_source_all += sqlitesources
libgda_source_all += libgda_virtual_sources
libgda_source_all += libgda_sql_parser_sources
libgda_source_all += libgda_psupport_sources
libgda_source_all += libgda_handlers_sources
libgda_source_all += libgda_threadwrapper_sources
libgda_source_all += libgda_binreloc_sources

inc_sqliteh = include_directories ('sqlite')
inc_sqliteh_dep = declare_dependency (include_directories : inc_sqliteh)

libgda_dep = [
	glib_dep,
	gobject_dep,
	gio_dep,
	gmodule_no_export_dep,
	libxml_dep,
	inc_rooth_dep,
	inc_libgdah_dep
	]

if sqlite_dep.found ()
	libgda_dep += sqlite_dep
endif

libgda_cargs = [
		'-include',
		meson.build_root() + '/config.h',
		'-DABI_VERSION='+project_abi_version,
		'-DABI_NAME='+project_abi_name,
	]

libgda_cargs += sqlite_cargs
libgda_cargs += binreloc_cargs

libgda = library(
	'gda-'+project_api_version,
	libgda_source_all,
	soversion : gda_current,
	version : gda_revision,
	dependencies : [libgda_dep,
			gda_keywordshash_dep,
			inc_sqliteh_dep,
			inc_libsql_parserh_dep],
	c_args : libgda_cargs,
	install : true
	)