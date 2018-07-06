threadwrapper_headers = files([
	'gda-worker.h',
	])
threadwrapper_headers_priv = files([
	'gda-connect.h'
	])

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