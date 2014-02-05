noinst_LTLIBRARIES = libgda_threadwrapper-6.0.la

TESTS = test-raw-itsignaler test-itsignaler test-blocking-itsignaler test-worker test-connect
check_PROGRAMS = test-raw-itsignaler test-itsignaler test-blocking-itsignaler test-worker test-connect
noinst_PROGRAMS = perf-itsignaler

if PLATFORM_WIN32
EXTRALDFLAGS=-lwsock32 -lws2_32
else
TESTS += test-raw-itsignaler-no-eventfd
check_PROGRAMS += test-raw-itsignaler-no-eventfd
noinst_PROGRAMS += perf-itsignaler-no-eventfd
EXTRALDFLAGS=
endif

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(srcdir)/.. \
	-I$(top_srcdir)/libgda/sqlite \
        -I$(top_builddir) \
        $(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS)

libgda_threadwrapper_headers = \
	gda-worker.h \
	gda-connect.h

libgda_threadwrapperincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/thread-wrapper
libgda_threadwrapperinclude_HEADERS=$(libgda_threadwrapper_headers)

libgda_threadwrapper_6_0_la_SOURCES = \
	$(libgda_threadwrapper_headers) \
	itsignaler.h \
	itsignaler.c \
	gda-worker.c \
	gda-connect.c

# test_itsignaler
test_itsignaler_CFLAGS = \
	$(COREDEPS_CFLAGS)

test_itsignaler_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-itsignaler.c

test_itsignaler_LDADD = \
	$(COREDEPS_LIBS)


# test_raw_itsignaler
test_raw_itsignaler_CFLAGS = \
	$(COREDEPS_CFLAGS)

test_raw_itsignaler_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-raw-itsignaler.c

test_raw_itsignaler_LDADD = \
	$(COREDEPS_LIBS) \
	$(EXTRALDFLAGS)


# test_raw_itsignaler_no_eventfd
test_raw_itsignaler_no_eventfd_CFLAGS = \
	-DFORCE_NO_EVENTFD \
	$(COREDEPS_CFLAGS)

test_raw_itsignaler_no_eventfd_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-raw-itsignaler.c

test_raw_itsignaler_no_eventfd_LDADD = \
	$(COREDEPS_LIBS)

# test_blocking_itsignaler
test_blocking_itsignaler_CFLAGS = \
	$(COREDEPS_CFLAGS)

test_blocking_itsignaler_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-blocking-itsignaler.c

test_blocking_itsignaler_LDADD = \
	$(COREDEPS_LIBS) \
	$(EXTRALDFLAGS)

# perf_itsignaler
perf_itsignaler_CFLAGS = \
	-DPERF \
	$(COREDEPS_CFLAGS)

perf_itsignaler_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-raw-itsignaler.c

perf_itsignaler_LDADD = \
	$(COREDEPS_LIBS) \
	$(EXTRALDFLAGS)


# perf_itsignaler_no_eventfd
perf_itsignaler_no_eventfd_CFLAGS = \
	-DFORCE_NO_EVENTFD \
	-DPERF \
	$(COREDEPS_CFLAGS)

perf_itsignaler_no_eventfd_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	test-raw-itsignaler.c

perf_itsignaler_no_eventfd_LDADD = \
	$(COREDEPS_LIBS)

# test_worker
test_worker_CFLAGS = \
	$(COREDEPS_CFLAGS)

test_worker_SOURCES = \
	itsignaler.h \
	itsignaler.c \
	gda-worker.h \
	gda-worker.c \
	test-worker.c

test_worker_LDADD = \
	$(COREDEPS_LIBS)

# test_connect
test_connect_CFLAGS = \
	$(COREDEPS_CFLAGS)

test_connect_SOURCES = \
	dummy-object.h \
	dummy-object.c \
	itsignaler.h \
	itsignaler.c \
	gda-connect.h \
	gda-connect.c \
	test-connect.c

test_connect_LDADD = \
	$(COREDEPS_LIBS)