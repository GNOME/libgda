docui_conf = configuration_data()
docui_conf.set('GDA_VERSION', project_version)
docui_conf.set('GDA_BUILDDATE', dates.stdout())

doc_version = configure_file(
	input: 'version.xml.in',
	output: 'version.xml',
	configuration: docui_conf
	)

gdaui_html_images = files([
        'vi-basic-form.png',
        'vi-combo.png',
        'vi-filter.png',
        'vi-login.png',
        'vi-raw-grid.png',
        'vi-server-op.png',
        'vi-cloud.png',
        'vi-data-entry.png',
        'vi-info.png',
        'vi-provider-selector.png',
        'vi-rte.png'
	])

libgdaui_doc_dep = declare_dependency(link_with: [libgdaui, libgda_ui_plugins])

gnome_module.gtkdoc(
        'libgdaui-'+project_api_version,
        main_xml: 'libgdaui-'+project_api_version+'-docs.xml',
        namespace: 'gdaui',
        mode: 'none',
        src_dir: join_paths(meson.source_root(),'libgda-ui'),
        dependencies: [libgda_doc_dep, libgda_dep, libgdaui_doc_dep],
        scan_args: [
                '--deprecated-guards="GDA_DISABLE_DEPRECATED"',
                '--rebuild-types'
        ],
        ignore_headers: [
                'gdaui-entry-password.h',
                'gdaui-data-cell-renderer-password.h'
        ],
        html_assets: gdaui_html_images,
        fixxref_args: [
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
                '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
        ],
        mkdb_args: [
                '--xml-mode'
        ],
        install: true
    )
