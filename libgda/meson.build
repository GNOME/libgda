inc_libgdah = include_directories ('.')
inc_libgdah_dep = declare_dependency (include_directories : inc_libgdah)


# Build auxiliary executables and libraries
subdir('sqlite')
subdir('sql-parser')
subdir('providers-support')
subdir('handlers')
subdir('thread-wrapper')
subdir('binreloc')

dtdf = files([
	'libgda-array.dtd',
	'libgda-paramlist.dtd',
	'libgda-server-operation.dtd',
	'libgda-ddl-creator.dtd'
	])

install_data(dtdf,
	install_dir: join_paths(get_option('datadir'),project_package,'dtd')
	)

# Internal information schema
install_data('information_schema.xml',
	install_dir: join_paths(get_option('datadir'), project_package)
	)

libgda_virtualh = configure_file(
		input: 'libgda.h.in',
		output : 'libgda.h',
		configuration : conf)


gda_marshalls = gnome_module.genmarshal('gda-marshal',prefix : '_gda_marshal',
					sources : 'gda-marshal.list')

gda_enum_headers = files ([
	'gda-connection.h',
	'gda-enums.h'
	])

gda_enums = gnome_module.mkenums_simple('gda-enum-types', sources: gda_enum_headers)

mkkeywordhashf = files([
	'sqlite/mkkeywordhash.c'
])

mkkeywordhash = executable ('mkkeywordhash', mkkeywordhashf)

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
        'gda-ddl-creator.h',
        'gda-ddl-table.h',
        'gda-ddl-view.h',
        'gda-ddl-column.h',
        'gda-ddl-fkey.h',
        'gda-ddl-base.h',
        'gda-ddl-buildable.h',
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
libgda_headers_all = []
libgda_headers_all += [
	libgda_headers,
	gda_enum_headers
	]

libgda_sources= files([
	'gda-attributes-manager.c',
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
        'gda-ddl-creator.c',
        'gda-ddl-table.c',
        'gda-ddl-view.c',
        'gda-ddl-column.c',
        'gda-ddl-fkey.c',
        'gda-ddl-base.c',
        'gda-ddl-buildable.c',
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

libgda_source_csvh = files([
	'csv.h'
	])
libgda_source_all = files([
	'libcsv.c',
	'dir-blob-op.h',
	'dir-blob-op.c',
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
	libgda_headers_all,
	gda_enums,
	gda_marshalls,
	libgda_source_csvh
	]
libgda_source_all += [
	sqlitesources,
	libgda_virtual_sources,
	libgda_sql_parser_sources,
	libgda_psupport_sources,
	libgda_handlers_sources,
	libgda_threadwrapper_sources,
	libgda_binreloc_sources
	]

ldap_cargs = []
if ldap_found
ldaph_sources = files ([
	'gda-data-model-ldap.h',
	'gda-tree-mgr-ldap.h'
	])
libgda_headers_all += ldaph_sources
ldapc_sources = files ([
	'gda-data-model-ldap.c',
	'gda-tree-mgr-ldap.c'
	])
libgda_source_all += ldapc_sources
ldap_cargs += [
	'-DHAVE_LDAP'
	]
endif

if lbdb_found
	lbdbh_sources = files ([
		'gda-data-model-bdb.h'
		])
	libgda_headers_all += lbdbh_sources
	lbdb_sources = files ([
		'gda-data-model-bdb.c',
		])
	libgda_source_all += lbdb_sources
	lbdb_cargs += ['-DBDB_LIB="'+lbdb_filename+'"']
endif

inc_sqliteh = include_directories ('sqlite')
inc_sqliteh_dep = declare_dependency (include_directories : inc_sqliteh)


libgda_dep += [inc_libgdah_dep]

libgda_cargs = [
		'-include',
		meson.build_root() + '/config.h',
		'-DABI_VERSION='+project_api_version,
		'-DABI_NAME='+project_api_name,
	]

libgda_cargs += sqlite_cargs
libgda_cargs += binreloc_cargs

libgda_dep += sqlite_deps


libgda = library(
	'gda-'+project_api_version,
	libgda_source_all,
	soversion : gda_current,
	version : gda_revision,
	dependencies : [
		libgda_dep,
		#ldap_deps,
		gda_keywordshash_dep,
		inc_sqliteh_dep,
		inc_libsql_parserh_dep
		],
	c_args : [
		libgda_cargs,
		ldap_cargs,
		lbdb_cargs
		],
	install : true
	)

# GObject Introspection
if girscanner.found()
gi_sources = [
	libgda_sources,
	libgda_headers,
	gda_enum_headers,
	psupport_gi_sources,
	threadwrapper_gi_sources,
	sql_parser_gi_sources,
	libgda_handlers_sources
	]


libgda_gir = gnome_module.generate_gir(libgda,
	sources: gi_sources,
	namespace: 'Gda',
	identifier_prefix: 'Gda',
	nsversion: project_api_version,
	header: 'libgda/libgda.h',
	dependencies : [
		libgda_dep,
		],
	includes: [
		'GLib-2.0',
		'GObject-2.0',
		'Gio-2.0',
		'libxml2-2.0'
		],
	install: true,
	install_dir_gir: join_paths(get_option('datadir'), 'gir-1.0'),
	install_dir_typelib: join_paths(get_option('libdir'), 'girepository-1.0'),
	)
	if vapigen.found()
	libgda_vala_deps = configure_file(
		input: 'libgda.deps.in',
		output: 'libgda-'+project_api_version+'.deps',
		configuration: tconf,
		install: true,
		install_dir: join_paths(get_option('datadir'),'vala','vapi')
		)
	libgda_vapi = custom_target(project_package,
		command: [vapigen,
			'--girdir='+meson.current_source_dir(),
			'--metadatadir='+meson.current_source_dir(),
			'--library='+project_package,
			'--directory='+meson.current_build_dir(),
			'--pkg=libxml-2.0',
			'--pkg=gio-2.0',
			'@INPUT0@', '@INPUT1@'
			],
		input: [libgda_gir[0],'Gda-'+project_api_version+'-custom.vala'],
		output: project_package+'.vapi',
		install: true,
		install_dir: join_paths(get_option('datadir'),'vala','vapi'),
		depends: libgda_gir
		)
	endif
endif

# Tests
tco = executable('test_cnc_open', 'test-cnc-open.c',
	dependencies: [
		libgda_dep,
		inc_rooth_dep
		],
	link_with: libgda,
	install: false
	)
test('OpenConnection', tco,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root(),
		'GDA_SHOW_PROVIDER_LOADING_ERROR=1'
		]
	)

tce = executable('test_cnc_exec', 'test-cnc-exec.c',
	c_args: [
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep
		],
	link_with: libgda,
	install: false
	)
test('OpenConnectionExecute', tce,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

tcm = executable('test_cnc_meta', 'test-cnc-meta.c',
	c_args: [
		'-DROOT_DIR="'+meson.source_root()+'"'
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep
		],
	link_with: libgda,
	install: false
	)
test('OpenConnectionMeta', tcm,
	env: [
		'GDA_TOP_SRC_DIR='+meson.source_root(),
		'GDA_TOP_BUILD_DIR='+meson.build_root()
		]
	)

# Thread Wrapper tests
ttw = executable('test-raw-itsignaler',
	test_raw_itsignaler_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('RawItSignaler', ttw)

ttws = executable('test-itsignaler',
	test_itsignaler_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('ItSignaler', ttws)

ttwbs = executable('test-blocking-itsignaler',
	test_block_itsignaler_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('BlockingItSignaler', ttwbs)

ttww = executable('test-worker test-connect',
	test_worker_sources,
	c_args: [
		'-include',
		meson.build_root() + '/config.h',
		],
	dependencies: [
		libgda_dep,
		inc_rooth_dep,
		inc_sqliteh_dep
		],
	install: false
	)
test('Worker', ttww)
