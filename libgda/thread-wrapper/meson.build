threadwrapper_headers = files([
	'gda-worker.h',
	])
threadwrapper_headers_priv = files([
	'gda-connect.h'
	])

install_headers(threadwrapper_headers + threadwrapper_headers_priv,
	install_dir: join_paths(get_option('includedir'),project_package,'libgda','thread-wrapper')
	)

threadwrapper_sources = files([
	'gda-worker.c',
	])

threadwrapper_sources_priv = files([
	'background.h',
	'background.c',
	'itsignaler.h',
	'itsignaler.c',
	'gda-connect.c'
	])
libgda_threadwrapper_sources = []
libgda_threadwrapper_sources += threadwrapper_sources
libgda_threadwrapper_sources += threadwrapper_sources_priv
libgda_threadwrapper_sources += threadwrapper_headers
libgda_threadwrapper_sources += threadwrapper_headers_priv

threadwrapper_gi_sources = []
threadwrapper_gi_sources += threadwrapper_sources
threadwrapper_gi_sources += threadwrapper_headers

test_raw_itsignaler_sources = files([
	'test-raw-itsignaler.c'
	])
test_raw_itsignaler_sources += libgda_threadwrapper_sources


test_itsignaler_sources = files([
	'test-itsignaler.c'
	])
test_itsignaler_sources += libgda_threadwrapper_sources


test_block_itsignaler_sources = files([
	'test-blocking-itsignaler.c'
	])
test_block_itsignaler_sources += libgda_threadwrapper_sources


test_worker_sources = files([
	'test-worker.c'
	])
test_worker_sources += libgda_threadwrapper_sources

test_connect_sources = files([
	'dummy-object.h',
	'dummy-object.c',
	'test-connect.c'
])

test_connect_sources += threadwrapper_sources_priv
test_connect_sources += threadwrapper_headers_priv
test_connect_sources += threadwrapper_sources
test_connect_sources += threadwrapper_headers

