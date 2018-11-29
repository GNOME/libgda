libgda_typesdoc = configure_file(
		input: 'libgda.types.in',
		output : 'libgda.types',
		configuration : conf)

gda_html_images = files([
	'DataModels.png',
	'architecture.png',
	'parts.png',
	'stmt-unknown.png',
	'stmt-select.png',
	'stmt-insert1.png',
	'stmt-insert2.png',
	'stmt-update.png',
	'stmt-compound.png',
	'information_schema.png',
	'MetaStore1.png',
	'MetaStore2.png',
	'i_s_data_types.png',
	'writable_data_model.png',
	'GdaDataModelIter.png',
	'data_validation_holder.png',
	'data_validation_proxy.png',
	'data_validation_set.png',
	'data_proxy1.png',
	'data_proxy2.png',
	'data_proxy3.png',
	'data_proxy4.png',
	'data_proxy5.png',
	'gda-sql-graph.png',
	'howto-exec.png',
	'GdaStatement.png',
	'parser_gen.png',
	'parser_prov.png',
	'tree-overview.png',
	'tree-overview2.png',
	'SqlIdentifiers.png',
	'thread-wrapper.png',
	'connection-status.png'
	])

doc_conf = configuration_data()
doc_conf.set('GDA_VERSION', project_version)
doc_conf.set('GDA_BUILDDATE', dates.stdout())

doc_version = configure_file(
	input: 'version.xml.in',
	output: 'version.xml',
	configuration: doc_conf
	)

doc_builddate= configure_file(
	input: 'builddate.xml.in',
	output: 'builddate.xml',
	configuration: doc_conf
	)

contentf = [
	doc_version,
	doc_builddate
	]
contentf += files([
	'fdl-appendix.sgml'
	])

# ignore_bdb = []
# if bdb_found
# else
# 	ignore_bdb += ['gda-data-model-bdb.h']
# endif

# gnome_module.gtkdoc(project_package,
# 	main_xml: project_package+'-docs.xml',
# 	scan_args: [
# 		'--deprecated-guards="GDA_DISABLE_DEPRECATED"',
# 		'--rebuild-types',
# 		'--ignore-headers',
# 		'gda-marshal.h', 'gda-custom-marshal.h', 'gda-enum-types.h', 'sqlite3.h',
# 		'csv.h', 'md5.h', 'jni-wrapper.h', 'gda-jdbc-recordset.h',
# 		'gda-jdbc-blob-op.h', 'gda-jdbc-provider.h', 'gda-jdbc-pstmt.h',
# 		'providers', 'tools control-center', 'tests', 'examples', 'plugins', 'tmp',
# 		ignore_bdb
# 		 ],
# 	src_dir: join_paths(meson.source_root(),'libgda'),
# 	mkdb_args: [
# 		'--xml-mode',
# 		'--output-format=xml',
# 		'--name-space=gda'
# 		],
# 	)
