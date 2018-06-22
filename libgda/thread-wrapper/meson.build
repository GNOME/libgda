libgda_threadwrapper_headers = files([
	'gda-worker.h',
	'gda-connect.h'
	])

libgda_threadwrapper_sources = files([
	'background.h',
	'background.c',
	'itsignaler.h',
	'itsignaler.c',
	'gda-worker.c',
	'gda-connect.c'
	])

libgda_threadwrapper_sources += libgda_threadwrapper_headers

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