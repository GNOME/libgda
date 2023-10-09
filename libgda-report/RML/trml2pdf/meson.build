trml2pdf_install_dir = join_paths(get_option('datadir'), project_package, 'gda_trml2pdf')

install_data('trml2pdf.py',
	install_dir: trml2pdf_install_dir,
	install_mode: 'rwxr-xr-x',
	)
trml2pdf = files(['__init__.py', 'utils.py', 'color.py'])
install_data(trml2pdf,
	install_dir: trml2pdf_install_dir
	)
