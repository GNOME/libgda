
marshal_code = gnome_module.genmarshal('gda-marshal',prefix : '_gda_marshal',
    sources : 'gda-marshal.list')

subdir('sql-parser')

libgda_headers = [
	'gda-attributes-manager.h',
	'gda-batch.h',
	'gda-blob-op.h', 
	'gda-column.h',
	'gda-config.h',
	'gda-connection-event.h',
	'gda-connection.h',
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
	'gda-enums.h', 
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
]

libgda_headers += marshal_code[1]

libgda_sources= [
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
	'gda-data-pivot.c' ]

libgda_sources += marshal_code[0]

libgda_source_all = [
	'csv.h',
	'libcsv.c',
	'dir-blob-op.h',
	'dir-blob-op.c',
	'gda-attributes-manager.c',
	libgda_sources,
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
	'gda-marshal.c',
	'gda-enum-types.c',
	'gda-server-operation-private.h',
	'gda-types.c',
	'gda-types.h',
	'gda-statement-priv.h',
	libgda_headers,
	'libgda.h' ]


libgda_dep = [glib_dep, gio_dep]

libgda_mapfile = 'libgda.map'
include_dir = include_directories()

asglib = shared_library(
  'libgda_6_0_la', libgda_sources + libgda_headers,
  soversion : gda_current,
  version : gda_revision,
  dependencies : libgda_dep,
  include_directories : include_dir,
  link_depends : libgda_mapfile,
  install : true)

















