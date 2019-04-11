/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifdef FORCE_NO_EVENTFD
  /* for testing purposes */
  #undef HAVE_EVENTFD
#endif

#define DEBUG_NOTIFICATION
#undef DEBUG_NOTIFICATION

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#include "itsignaler.h"
#ifndef NOBG
  #include "background.h"
#endif
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <glib-object.h>

/* optimizations */
static guint created_objects = 0; /* counter of all the ITSignaler objects ever created */

#ifdef G_OS_WIN32
  #define INVALID_SOCK INVALID_SOCKET
  #define SIGNALER_PORT 5906
#else
  #define _GNU_SOURCE
  #include <sys/types.h>
  #include <fcntl.h>
  #ifdef HAVE_EVENTFD
    #include <sys/eventfd.h>
  #else

  #endif
  #define INVALID_SOCK -1
  #include <poll.h>
#endif

typedef struct {
	gpointer data;
	GDestroyNotify destroy_func;
} NotificationData;

/*
 * Threads synchronization with notifications
 *
 * Use a GAsyncQueue to pass notifications from one thread to the other.
 *
 * Windows specific implementation:
 *  Use a client/server pair of sockets, waiting can be done on the reading socket
 *
 * Unix specific implementation:
 *  If HAVE_EVENTFD is defined (Linux extension), then use eventfd(), else use a pair of file descriptors obtained
 *  using pipe()
 *
 */
struct _ITSignaler {
	guint8       broken; /* TRUE if the object has suffered an unrecoverable error */

#ifdef HAVE_FORK
	/* detect forked process */
	pid_t        pid;
#endif

	/* data queue */
	GAsyncQueue *data_queue;

	/* signaling mechanism */
#ifdef G_OS_WIN32
	SOCKET       socks[2]; /* [0] for reading and [1] for writing */
#else
  #ifdef HAVE_EVENTFD
	int          event_fd;
  #else
	int          fds[2]; /* [0] for reading and [1] for writing */
	GIOChannel  *ioc;
  #endif
#endif

	/* reference count, thread safe */
	guint        ref_count;
	GMutex       mutex;
};

#define itsignaler_lock(x) g_mutex_lock(& (((ITSignaler*)x)->mutex))
#define itsignaler_unlock(x) g_mutex_unlock(& (((ITSignaler*)x)->mutex))

/**
 * itsignaler_ref:
 * @its: (nullable): a #ITSignaler object
 *
 * Increases the reference count of @its. If @its is %NULL, then nothing happens.
 *
 * This function can be called from any thread.
 *
 * Returns: @its
 */
ITSignaler *
itsignaler_ref (ITSignaler *its)
{
	if (its) {
		itsignaler_lock (its);
		its->ref_count++;
#ifdef DEBUG_NOTIFICATION
		g_print ("[I] ITSignaler %p ++: %u\n", its, its->ref_count);
#endif
		itsignaler_unlock (its);
	}
	return its;
}

static void
cleanup_signaling (ITSignaler *its)
{
#ifdef G_OS_WIN32
	int rc;
	if (its->socks[1] != INVALID_SOCKET) {
		struct linger so_linger = { 1, 0 };
		rc = setsockopt (its->socks[1], SOL_SOCKET, SO_LINGER,
				 (char *)&so_linger, sizeof (so_linger));
		g_assert (rc != SOCKET_ERROR);
		rc = closesocket (its->socks[1]);
		g_assert (rc != SOCKET_ERROR);
		its->socks [1] = INVALID_SOCKET;
	}
	if (its->socks[0] != INVALID_SOCKET) {
		rc = closesocket (its->socks[0]);
		g_assert (rc != SOCKET_ERROR);
		its->socks [0] = INVALID_SOCKET;
	}
#else
  #ifdef HAVE_EVENTFD
	if (its->event_fd != INVALID_SOCK) {
		g_assert (close (its->event_fd) == 0);
		its->event_fd = INVALID_SOCK;
	}
  #else
	if (its->fds[0] != INVALID_SOCK) {
		g_assert (close (its->fds[0]) == 0);
		its->fds[0] = INVALID_SOCK;
	}
	if (its->fds[1] != INVALID_SOCK) {
		g_assert (close (its->fds[1]) == 0);
		its->fds[1] = INVALID_SOCK;
	}
  #endif
#endif
}

#ifndef NOBG
static void
itsignaler_reset (ITSignaler *its)
{
	g_assert (its->ref_count == 0);
#ifdef G_OS_WIN32
	guint8 value;
	ssize_t nr;
	for (nr = recv (its->socks[0], (char*) &value, sizeof (value), 0);
	     nr >= 0;
	     nr = recv (its->socks[0], (char*) &value, sizeof (value), 0));
	g_assert (nr == -1);
#else
  #ifdef HAVE_EVENTFD
	guint64 nbnotif;
	ssize_t nr = read (its->event_fd, &nbnotif, sizeof (nbnotif));
	nr = read (its->event_fd, &nbnotif, sizeof (nbnotif));
	g_assert (nr == -1);
  #else
	guint8 value;
	ssize_t nr;
	for (nr = read (its->fds[0], &value, sizeof (value));
	     nr >= 0;
	     nr = read (its->fds[0], &value, sizeof (value)));
	g_assert (nr == -1);
  #endif
#endif
	NotificationData *nd;
	for (nd = g_async_queue_try_pop (its->data_queue);
	     nd;
	     nd = g_async_queue_try_pop (its->data_queue)) {
		if (nd->data && nd->destroy_func)
			nd->destroy_func (nd->data);
	}
}
#endif

/*
 * This function requires that @its be locked using itsignaler_lock()
 */
static void
itsignaler_free (ITSignaler *its)
{
	/* destroy @its */
	GMutex *m = &(its->mutex);
	cleanup_signaling (its);

	/* clear queue's contents */
	g_async_queue_unref (its->data_queue);

#ifdef DEBUG_NOTIFICATION
	g_print ("[I] Destroyed ITSignaler %p\n", its);
#endif
	g_mutex_unlock (m);
	g_mutex_clear (m);
#ifndef NOBG
	bg_update_stats (BG_DESTROYED_ITS);
#endif

	g_free (its);
}

void
_itsignaler_unref (ITSignaler *its, gboolean give_to_bg)
{
	g_assert (its);

	itsignaler_lock (its);
	its->ref_count--;
#ifdef DEBUG_NOTIFICATION
	g_print ("[I] ITSignaler %p --: %u\n", its, its->ref_count);
#endif
	if (its->ref_count == 0) {
#ifndef NOBG
		/* destroy or store as spare */
		if (!its->broken && give_to_bg) {
			itsignaler_reset (its);
			its->ref_count++;
			bg_set_spare_its (its);
			itsignaler_unlock (its);
		}
		else
			itsignaler_free (its);
#else
		itsignaler_free (its);
#endif
	}
	else
		itsignaler_unlock (its);
}

/**
 * itsignaler_unref:
 * @its: (nullable): a #ITSignaler object
 *
 * Decrease the reference count of @its; when the rerefence count reaches zero, the object
 * is freed. If @its is %NULL, then nothing happens.
 *
 * This function can be called from any thread.
 */
void
itsignaler_unref (ITSignaler *its)
{
	if (its)
		_itsignaler_unref (its, TRUE);
}

void
_itsignaler_bg_unref (ITSignaler *its)
{
	g_assert (its);
	g_assert (its->ref_count == 1);

	_itsignaler_unref (its, FALSE);
}

static void
notification_data_free (NotificationData *nd)
{
	if (nd->data && nd->destroy_func)
		nd->destroy_func (nd->data);
	g_free (nd);
}

/**
 * itsignaler_new:
 *
 * Creates a new #ITSignaler object.
 *
 * Returns: a new #ITSignaler, or %NULL if an error occurred. Use itsignaler_unref() when not needed anymore.
 */
ITSignaler *
itsignaler_new (void)
{
	ITSignaler *its;
#ifndef NOBG
	its = bg_get_spare_its ();
	if (its)
		return its;
#endif

	gboolean err = FALSE;
	its = g_new0 (ITSignaler, 1);
	its->ref_count = 1;
	its->broken = FALSE;

#ifdef G_OS_WIN32
	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;
	memset (&sd, 0, sizeof (sd));
	memset (&sa, 0, sizeof (sa));

	InitializeSecurityDescriptor (&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl (&sd, TRUE, 0, FALSE);

	sa.nLength = sizeof (SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = &sd;

	DWORD dwrc;
	HANDLE sync = CreateEvent (&sa, FALSE, TRUE, TEXT ("Global\\gda-signaler-port"));
	if (!sync && (GetLastError () == ERROR_ACCESS_DENIED))
		sync = OpenEvent (SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, TEXT ("Global\\gda-signaler-port"));
	if (!sync ||
	    ((dwrc = WaitForSingleObject (sync, INFINITE)) != WAIT_OBJECT_0)) {
		err = TRUE;
		goto next;
	}

	/*  Windows has no 'socketpair' function. CreatePipe is no good as pipe
	 *  handles cannot be polled on. Here we create the socketpair by hand
	 */
	its->socks [0] = INVALID_SOCKET;
	its->socks [1] = INVALID_SOCKET;

	/* Initialize sockets */
	static gboolean init_done = FALSE;
	if (!init_done) {
		WORD version_requested = MAKEWORD (2, 2);
		WSADATA wsa_data;
		int rc = WSAStartup (version_requested, &wsa_data); // FIXME: call WSACleanup() somehow...
		if ((rc != 0) || (LOBYTE (wsa_data.wVersion) != 2) || (HIBYTE (wsa_data.wVersion) != 2)) {
			err = TRUE;
			goto next;
		}
		init_done = TRUE;
	}

	/* Create listening socket */
	SOCKET listener;
	listener = socket (AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET) {
		err = TRUE;
		goto next;
	}

	/* preventing sockets to be inherited by child processes */
	BOOL brc = SetHandleInformation ((HANDLE) listener, HANDLE_FLAG_INHERIT, 0);
	if (!brc) {
		closesocket (listener);
		err = TRUE;
		goto next;
	}

	/* Set SO_REUSEADDR and TCP_NODELAY on listening socket */
	BOOL so_reuseaddr = 1;
	int rc = setsockopt (listener, SOL_SOCKET, SO_REUSEADDR,
			     (char *)&so_reuseaddr, sizeof (so_reuseaddr));
	if (rc == SOCKET_ERROR) {
		closesocket (listener);
		err = TRUE;
		goto next;
	}

	BOOL tcp_nodelay = 1;
	rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY,
			 (char *)&tcp_nodelay, sizeof (tcp_nodelay));
	if (rc == SOCKET_ERROR) {
		closesocket (listener);
		err = TRUE;
		goto next;
	}

	/* Bind listening socket to signaler port */
	struct sockaddr_in addr;
	memset (&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	addr.sin_port = htons (SIGNALER_PORT);
	rc = bind (listener, (const struct sockaddr*) &addr, sizeof (addr));
	if (rc == SOCKET_ERROR) {
		closesocket (listener);
		err = TRUE;
		goto next;
	}

	/* Listen for incomming connections */
	rc = listen (listener, 1);
	if (rc == SOCKET_ERROR) {
		closesocket (listener);
		err = TRUE;
		goto next;
	}

	/* Create the writer socket */
	its->socks[1] = WSASocket (AF_INET, SOCK_STREAM, 0, NULL, 0,  0);
	if (its->socks[1] == INVALID_SOCKET) {
		err = TRUE;
		goto next;
	}

	/* Preventing sockets to be inherited by child processes */
	brc = SetHandleInformation ((HANDLE) its->socks[1], HANDLE_FLAG_INHERIT, 0);
	if (!brc) {
		err = TRUE;
		goto next;
	}

	/* Set TCP_NODELAY on writer socket */
	rc = setsockopt (its->socks[1], IPPROTO_TCP, TCP_NODELAY,
			 (char *)&tcp_nodelay, sizeof (tcp_nodelay));
	if (rc == SOCKET_ERROR) {
		err = TRUE;
		goto next;
	}

	/* Connect writer to the listener */
	rc = connect (its->socks[1], (struct sockaddr*) &addr, sizeof (addr));

	/* Accept connection from writer */
	its->socks[0] = accept (listener, NULL, NULL);

	/* We don't need the listening socket anymore. Close it */
	rc = closesocket (listener);
	if (rc == SOCKET_ERROR) {
		err = TRUE;
		goto next;
	}

	/* Exit the critical section */
	brc = SetEvent (sync);
	if (!brc) {
		err = TRUE;
		goto next;
	}

	if (its->socks[0] == INVALID_SOCKET) {
		err = TRUE;
		closesocket (its->socks[1]);
		its->socks[1] = INVALID_SOCKET;
		goto next;
	}

	/* Set non blocking mode */
	u_long nonblock = 1;
	rc = ioctlsocket (its->socks[0], FIONBIO, &nonblock);
	if (rc == SOCKET_ERROR) {
		err = TRUE;
		goto next;
	}

	/* Preventing sockets to be inherited by child processes */
	brc = SetHandleInformation ((HANDLE) its->socks[0], HANDLE_FLAG_INHERIT, 0);
	if (!brc) {
		err = TRUE;
		goto next;
	}
#else
  #ifdef HAVE_EVENTFD
	its->event_fd = eventfd (0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (its->event_fd == INVALID_SOCK) {
		err = TRUE;
		goto next;
	}
  #else /* HAVE_EVENTFD */
	if (pipe (its->fds) != 0) {
		err = TRUE;
		goto next;
	}

	/* set read to non blocking */
	int flags;
	flags = fcntl (its->fds[0], F_GETFL, 0);
	if (flags < 0)
		err = TRUE;
	else {
		flags = flags | O_NONBLOCK;
    #ifdef HAVE_FD_CLOEXEC
		flags = flags | FD_CLOEXEC;
    #endif
		if (fcntl (its->fds[0], F_SETFL, flags) < 0)
			err = TRUE;
	}
  #endif /* HAVE_EVENTFD */
#endif /* G_OS_WIN32 */

 next:
	if (err) {
		cleanup_signaling (its);
		g_free (its);
		return NULL;
	}

	/* finish init */
	its->data_queue = g_async_queue_new_full ((GDestroyNotify) notification_data_free);
	g_mutex_init (&(its->mutex));

#ifdef HAVE_FORK
	its->pid = getpid ();
#endif

	if (err) {
		its->broken = TRUE;
		itsignaler_unref (its);
		return NULL;
	}

#ifdef DEBUG_NOTIFICATION
	g_print ("[I] Created ITSignaler %p\n", its);
#endif

	created_objects++;
#ifndef NOBG
	bg_update_stats (BG_CREATED_ITS);
#endif
	return its;
}

#ifdef G_OS_WIN32
/**
 * itsignaler_windows_get_poll_fd:
 * @its: a #ITSignaler object
 *
 * Get the socket descriptor associated to @its.
 *
 * Returns: the file descriptor number, or -1 on error
 */
SOCKET
itsignaler_windows_get_poll_fd (ITSignaler *its)
{
	g_return_val_if_fail (its, -1);
	return its->socks[0];
}

#else

/**
 * itsignaler_unix_get_poll_fd:
 * @its: a #ITSignaler object
 *
 * Get the file descriptor associated to @its.
 *
 * Returns: the file descriptor number, or -1 on error
 */
int
itsignaler_unix_get_poll_fd (ITSignaler *its)
{
	g_return_val_if_fail (its, -1);

#ifdef HAVE_EVENTFD
	return its->event_fd;
#else
	return its->fds[0];
#endif
}
#endif /* G_OS_WIN32 */

/**
 * itsignaler_push_notification:
 * @its: a #ITSignaler pointer
 * @data: a pointer to some data.
 * @destroy_func: (nullable): a function to be called to free @data, or %NULL
 *
 * Use this function to push a notification.
 *
 * Note that @data will be passes AS-IS to the thread which calls itsignaler_pop_notification(), any memory allocation
 * must be handled correctly by the caller. However, in case itsignaler_unref() is called while there are still some
 * undelivered notifications, each notification's data will be freed using the @destroy_func which was specified when
 * itsignaler_push_notification() was called (note that no warning of any sort will be shown if @destroy_func is %NULL
 * and some notification data should have been freed).
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
itsignaler_push_notification (ITSignaler *its, gpointer data, GDestroyNotify destroy_func)
{
	g_return_val_if_fail (its, FALSE);
	g_return_val_if_fail (data, FALSE);

	if (its->broken)
		return FALSE;

#ifdef HAVE_FORK
	if (unlikely (its->pid != getpid ()))
		return FALSE;
#endif

#ifdef DEBUG_NOTIFICATION_FORCE
	/* force an error */
	static guint c = 0;
	c++;
	if (c == 4)
		goto onerror;
#endif

	/* push notification to queue */
	NotificationData *nd;
	nd = g_new (NotificationData, 1);
	nd->data = data;
	nd->destroy_func = destroy_func;
  g_async_queue_lock (its->data_queue);
	g_async_queue_push_unlocked (its->data_queue, nd);
  g_async_queue_unlock (its->data_queue);

	/* actual notification */
#ifdef G_OS_WIN32
	const guint8 value = 1;
	ssize_t nw;
	nw = send (its->socks[1], (char*) &value, sizeof (value), 0);
	if (nw != sizeof (value))
		goto onerror; /* Error */
#else
  #ifdef HAVE_EVENTFD
	const uint64_t inc = 1;
	ssize_t nw;
	nw = write (its->event_fd, &inc, sizeof (inc));
	if (nw != sizeof (inc))
		goto onerror; /* Error */
  #else
	const guint8 value = 1;
	ssize_t nw;
	nw = write (its->fds[1], &value, sizeof (value));
	if (nw != sizeof (value))
		goto onerror; /* Error */
  #endif
#endif

	return TRUE;

 onerror:
	its->broken = TRUE;
	cleanup_signaling (its);

#ifdef DEBUG_NOTIFICATION
	g_print ("[I] %s(): returned FALSE\n", __FUNCTION__);
	g_print ("[I] ITSignaler will not be useable anymore\n");
#endif
	return FALSE;
}

static gpointer
itsignaler_pop_notification_non_block (ITSignaler *its)
{
	g_return_val_if_fail (its != NULL, NULL);

#ifdef G_OS_WIN32
	guint8 value;
	ssize_t nr = recv (its->socks[0], (char*) &value, sizeof (value), 0);
	if (nr == -1)
		return NULL; /* nothing to read */
	else
		g_assert (nr == sizeof (value));
#else
  #ifdef HAVE_EVENTFD
	guint64 nbnotif;
	ssize_t nr = read (its->event_fd, &nbnotif, sizeof (nbnotif));
	if (nr == -1) {
		return NULL; /* nothing to read */
	}
	else
		g_assert (nr == sizeof (nbnotif));
	nbnotif --;

	if (nbnotif > 0) {
		/* some other notifications need to be processed */
		ssize_t nw;
		nw = write (its->event_fd, &nbnotif, sizeof (nbnotif));
		if (nw != sizeof (nbnotif)) {
			close (its->event_fd);
			its->event_fd = INVALID_SOCK;
			its->broken = TRUE;
		}
	}
  #else
	guint8 value;
	ssize_t nr = read (its->fds[0], &value, sizeof (value));
	if (nr == -1)
		return NULL; /* nothing to read */
	else
		g_assert (nr == sizeof (value));
  #endif
#endif

	/* actual notification contents */
	NotificationData *nd;
  g_async_queue_lock (its->data_queue);
	nd = (NotificationData*) g_async_queue_pop_unlocked (its->data_queue);
  g_async_queue_unlock (its->data_queue);
	if (nd) {
		gpointer retval;
		retval = nd->data;
		g_free (nd);
		return retval;
	}
	else
		return NULL;
}

/**
 * itsignaler_pop_notification:
 * @its: a #ITSignaler object
 * @timeout_ms: if set to %0, then the function returns immediately if there is no notification, if set to a negative value, then this function blocks until a notification becomes available, otherwise maximum number of miliseconds to wait for a notification.
 *
 * Use this function from the thread to be signaled, to fetch any pending notification. If no notification is available,
 * then this function returns immediately %NULL. It's up to the caller to free the returned data.
 *
 * Returns: a pointer to some data which has been pushed by the notifying thread using itsignaler_push_notification(), or %NULL
 * if no notification is present.
 */
gpointer
itsignaler_pop_notification (ITSignaler *its, gint timeout_ms)
{
	g_return_val_if_fail (its, NULL);

	if (timeout_ms == 0)
		return itsignaler_pop_notification_non_block (its);

#ifdef G_OS_WIN32
	struct timeval *timeout_ptr = NULL;
	fd_set fds;
	SOCKET sock;

	if (timeout_ms > 0) {
		struct timeval timeout;
		timeout.tv_sec = timeout_ms / 1000;
		timeout.tv_usec = (timeout_ms - (timeout.tv_sec * 1000)) * 1000;
		timeout_ptr = &timeout;
	}
	sock = itsignaler_windows_get_poll_fd (its);
	FD_ZERO (&fds);
	FD_SET (sock, &fds);

	int res;
	res = select (1, &fds, 0, 0, timeout_ptr);
	if (res == SOCKET_ERROR)
		return NULL;
	else if (res > 0)
		return itsignaler_pop_notification_non_block (its);
#else
	struct pollfd fds[1];
	fds[0].fd = itsignaler_unix_get_poll_fd (its);
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	int res;
	res = poll (fds, 1, timeout_ms);
	if (res == -1)
		return NULL;
	else if (res > 0) {
		if (fds[0].revents | POLLIN)
			return itsignaler_pop_notification_non_block (its);
		else
			return NULL;
	}
#endif

	return NULL;
}

/*
 * GSource integration
 */
typedef struct {
	GSource     source;
	ITSignaler *its; /* reference held */
#ifndef G_OS_WIN32
	GPollFD     pollfd;
#endif
} ITSSource;

static gboolean its_source_prepare  (GSource *source, gint *timeout_);
static gboolean its_source_check    (GSource *source);
static gboolean its_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data);
static void     its_source_finalize (GSource *source);

GSourceFuncs funcs = {
	its_source_prepare,
	its_source_check,
	its_source_dispatch,
	its_source_finalize,
  NULL,
  NULL
};

/**
 * itsignaler_create_source:
 * @its: a #ITSignaler object
 *
 * Create a new #GSource for @its.
 *
 * The source will not initially be associated with any #GMainContext and must be added to one
 * with g_source_attach() before it will be executed.
 *
 * Returns: a new #GSource.
 */
GSource *
itsignaler_create_source (ITSignaler *its)
{
	g_return_val_if_fail (its, NULL);

	GSource *source;
	ITSSource *isource;
	source = g_source_new (&funcs, sizeof (ITSSource));
	isource = (ITSSource*) source;
	isource->its = itsignaler_ref (its);

#ifdef G_OS_WIN32
	SOCKET sock;
	GIOChannel *channel;
	sock = itsignaler_windows_get_poll_fd (its);
	channel = g_io_channel_win32_new_socket (sock);
	GSource *child_source;
	child_source = g_io_create_watch (channel, G_IO_IN | G_IO_HUP | G_IO_ERR);
	g_source_add_child_source (source, child_source);
	g_source_set_dummy_callback (child_source);
	g_io_channel_unref (channel);
	g_source_unref (child_source);
#else
	isource->pollfd.fd = itsignaler_unix_get_poll_fd (its);
	isource->pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	isource->pollfd.revents = 0;
	g_source_add_poll (source, &(isource->pollfd));
#endif

	return source;
}

static gboolean
its_source_prepare (G_GNUC_UNUSED GSource *source, gint *timeout_)
{
	*timeout_ = -1;
	return FALSE;
}

static gboolean
its_source_check (GSource *source)
{
	ITSSource *isource = (ITSSource*) source;

#ifndef G_OS_WIN32
	if (isource->pollfd.revents & G_IO_IN)
		return TRUE;
	
	if (isource->pollfd.revents & (G_IO_HUP | G_IO_ERR)) {
		g_source_remove_poll (source, &(isource->pollfd));
		isource->its->broken = TRUE;
		cleanup_signaling (isource->its);
		g_warning ("ITSignaler %p: ERROR HUP or ERR!\n", isource->its);
	}
#endif
	return FALSE;
}

static gboolean
its_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	ITSSource *isource = (ITSSource*) source;
	ITSignalerFunc func;
	func = (ITSignalerFunc) callback;

	gboolean retval;
	itsignaler_ref (isource->its);
	retval = func (user_data);
	itsignaler_unref (isource->its);
	return retval;
}

static
void its_source_finalize (GSource *source)
{
	ITSSource *isource = (ITSSource*) source;

	itsignaler_unref (isource->its);
}

/**
 * itsignaler_add:
 * @its: a #ITSignaler object
 * @context: (nullable): a GMainContext (if %NULL, the default context will be used).
 * @func: callback function to be called when a notification is ready
 * @data: data to pass to @func
 * @notify: (nullable): a function to call when data is no longer in use, or NULL.
 *
 * Have @its call @func (with @data) in the context of @context. Remove using itsignaler_remove(). This function
 * is similar to itsignaler_create_source() but is packaged for easier usage.
 *
 * Use itsignaler_remove() to undo this function.
 *
 * Returns: the ID (greater than 0) for the source within the #GMainContext.
 */
guint
itsignaler_add (ITSignaler *its, GMainContext *context, ITSignalerFunc func, gpointer data, GDestroyNotify notify)
{
	GSource *source;
	guint id;

	g_return_val_if_fail (its, 0);

	source = itsignaler_create_source (its);
	if (!source)
		return 0;

	g_source_set_callback (source, G_SOURCE_FUNC (func), data, notify);
	id = g_source_attach (source, context);
	g_source_unref (source);

	return id;
}

/**
 * itsignaler_remove:
 * @its: a #ITSignaler object
 * @context: (nullable): a GMainContext (if NULL, the default context will be used).
 * @id: the ID of the source as returned by itsignaler_add()
 *
 * Does the reverse of itsignaler_add().
 *
 * Returns: %TRUE if the source has been removed
 */
gboolean
itsignaler_remove (G_GNUC_UNUSED ITSignaler *its, GMainContext *context, guint id)
{
	GSource *source;
	source = g_main_context_find_source_by_id (context, id);
	if (source) {
		g_source_destroy (source);
		return TRUE;
	}
	else
		return FALSE;
}
