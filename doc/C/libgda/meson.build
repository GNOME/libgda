subdir('xml')

libgda_typesdoc = configure_file(
		input: 'libgda.types.in',
		output : 'libgda.types',
		configuration : conf)

gda_html_images = [
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
	]

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
	'fdl-appendix.sgml',
        'author-list.xml', 
        'installation.xml'
	])

# ignore_bdb = []
# if bdb_found
# else
#      ignore_bdb += ['gda-data-model-bdb.h']
# endif

libgda_doc_dep = declare_dependency(link_with: [libgda])
glib_prefix = glib_dep.get_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')


gnome_module.gtkdoc(
        project_package,
        main_xml: project_package+'-docs.xml',
        namespace: 'gda',
        mode: 'none',
        src_dir: [meson.project_source_root()/'libgda', meson.project_build_root()/'libgda'],
        dependencies: [libgda_doc_dep, libgda_dep],
        scan_args: [
                '--deprecated-guards="GDA_DISABLE_DEPRECATED"',
                '--rebuild-types'
        ],
        content_files: contentf,
        html_assets: gda_html_images,
        fixxref_args: [
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
        ],
        ignore_headers: [
                'gda-data-model-bdb.h',
                'dummy-object.h',
                'sqlite3.h'
        ],
        mkdb_args: [
                '--xml-mode'
        ],
        install: true
    )
