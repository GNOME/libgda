/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "../tools-utils.h"
#include "browser-connection.h"
#include <libgda/thread-wrapper/gda-thread-wrapper.h>
#include "support.h"
#include "marshal.h"
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-sql-builder.h>
#include <libgda-ui/gdaui-enums.h>
#include "../config-info.h"
#include "browser-virtual-connection.h"
#include <sqlite/virtual/gda-virtual-connection.h>

#include "browser-connection-priv.h"

#define CHECK_RESULTS_SHORT_TIMER 200
#define CHECK_RESULTS_LONG_TIMER 2

typedef struct {
	GObject *result;
	GError  *error;
	GdaSet  *last_inserted_row;
} StatementResult;

static void
statement_result_free (StatementResult *res)
{
	if (res->result)
		g_object_unref (res->result);
	if (res->last_inserted_row)
		g_object_unref (res->last_inserted_row);
	g_clear_error (&(res->error));
	g_free (res);
}

/* signals */
enum {
	BUSY,
	META_CHANGED,
	FAV_CHANGED,
	TRANSACTION_STATUS_CHANGED,
	TABLE_COLUMN_PREF_CHANGED,
	LAST_SIGNAL
};

gint browser_connection_signals [LAST_SIGNAL] = { 0, 0, 0, 0, 0 };

/* wrapper jobs handling */
static gboolean check_for_wrapper_result (BrowserConnection *bcnc);

typedef enum {
	JOB_TYPE_META_STORE_UPDATE,
	JOB_TYPE_META_STRUCT_SYNC,
	JOB_TYPE_STATEMENT_EXECUTE,
	JOB_TYPE_CALLBACK
} JobType;

typedef struct {
	guint    job_id;
	JobType  job_type;
	gchar   *reason;
	
	/* the following may be %NULL for stmt execution and meta store updates */
	BrowserConnectionJobCallback callback;
	gpointer cb_data;
} WrapperJob;

static void
wrapper_job_free (WrapperJob *wj)
{
	g_free (wj->reason);
	g_free (wj);
}

#ifdef GDA_DEBUG_MUTEX
static void
my_lock (GMutex *mutex, gint where)
{
	GTimer *timer;
	g_print ("wait to lock %p (th %p)@line %d\n", mutex, g_thread_self(), where);
	timer = g_timer_new ();
	g_mutex_lock (mutex);
	g_timer_stop (timer);

	if (g_timer_elapsed (timer, NULL) > 2.0)
		g_print ("WARN: locking %p (th %p)@line %d: %02f\n", mutex, g_thread_self(), where,
			 g_timer_elapsed (timer, NULL));
	g_print ("tmp LOC %p (th %p)@line %d took %02f\n", mutex, g_thread_self(), where,
		 g_timer_elapsed (timer, NULL));
	g_timer_destroy (timer);
}
static void
my_unlock (GMutex *mutex, gint where)
{
	g_mutex_unlock (mutex);
	g_print ("tmp UNL %p (th %p)@line %d\n", mutex, g_thread_self(), where);
}

#if GLIB_CHECK_VERSION(2,31,7)
#define MUTEX_LOCK(bcnc) g_mutex_lock (&((bcnc)->priv->mstruct_mutex))
#define MUTEX_UNLOCK(bcnc) g_mutex_unlock (&((bcnc)->priv->mstruct_mutex))
#else
#define MUTEX_LOCK(bcnc) my_lock ((bcnc)->priv->p_mstruct_mutex,__LINE__)
#define MUTEX_UNLOCK(bcnc) my_unlock ((bcnc)->priv->p_mstruct_mutex,__LINE__)
#endif
#else /* GDA_DEBUG_MUTEX */
#if GLIB_CHECK_VERSION(2,31,7)
#define MUTEX_LOCK(bcnc) g_mutex_lock (&((bcnc)->priv->mstruct_mutex))
#define MUTEX_UNLOCK(bcnc) g_mutex_unlock (&((bcnc)->priv->mstruct_mutex))
#else
#define MUTEX_LOCK(bcnc) g_mutex_lock ((bcnc)->priv->p_mstruct_mutex)
#define MUTEX_UNLOCK(bcnc) g_mutex_unlock ((bcnc)->priv->p_mstruct_mutex)
#endif
#endif /* GDA_DEBUG_MUTEX */

/*
 * Returns: %TRUE if current timer should be removed
 */
static gboolean
setup_results_timer (BrowserConnection *bcnc)
{
	gboolean short_timer = TRUE;

	if (bcnc->priv->ioc_watch_id != 0)
		return FALSE; /* nothing to do, we use notifications */

	bcnc->priv->nb_no_job_waits ++;
	if (bcnc->priv->nb_no_job_waits > 100)
		short_timer = FALSE;

	if ((bcnc->priv->wrapper_results_timer > 0) &&
	    (bcnc->priv->long_timer != short_timer))
		return FALSE; /* nothing to do, timer already correctlyset up */

	/* switch to a short/long timer to check for results */
	if (bcnc->priv->long_timer == short_timer)
		g_source_remove (bcnc->priv->wrapper_results_timer);

	bcnc->priv->long_timer = !short_timer;
	bcnc->priv->wrapper_results_timer = g_timeout_add (short_timer ? CHECK_RESULTS_SHORT_TIMER : CHECK_RESULTS_LONG_TIMER,
							   (GSourceFunc) check_for_wrapper_result,
							   bcnc);
	bcnc->priv->nb_no_job_waits = 0;
	return TRUE;
}

/*
 * Pushes a job which has been asked to be exected in a sub thread using gda_thread_wrapper_execute()
 */
static void
push_wrapper_job (BrowserConnection *bcnc, guint job_id, JobType job_type, const gchar *reason,
		  BrowserConnectionJobCallback callback, gpointer cb_data)
{
	/* handle timers if necessary */
	setup_results_timer (bcnc);

	/* add WrapperJob structure */
	WrapperJob *wj;
	wj = g_new0 (WrapperJob, 1);
	wj->job_id = job_id;
	wj->job_type = job_type;
	if (reason)
		wj->reason = g_strdup (reason);
	wj->callback = callback;
	wj->cb_data = cb_data;

	bcnc->priv->wrapper_jobs = g_slist_append (bcnc->priv->wrapper_jobs, wj);

	if (! bcnc->priv->wrapper_jobs->next)
		g_signal_emit (bcnc, browser_connection_signals [BUSY], 0, TRUE, wj->reason);
}

static void
pop_wrapper_job (BrowserConnection *bcnc, WrapperJob *wj)
{
	bcnc->priv->wrapper_jobs = g_slist_remove (bcnc->priv->wrapper_jobs, wj);
	wrapper_job_free (wj);
	g_signal_emit (bcnc, browser_connection_signals [BUSY], 0, FALSE, NULL);
}


/* 
 * Main static functions 
 */
static void browser_connection_class_init (BrowserConnectionClass *klass);
static void browser_connection_init (BrowserConnection *bcnc);
static void browser_connection_dispose (GObject *object);
static void browser_connection_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void browser_connection_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum {
        PROP_0,
        PROP_GDA_CNC
};

GType
browser_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (BrowserConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_connection_class_init,
			NULL,
			NULL,
			sizeof (BrowserConnection),
			0,
			(GInstanceInitFunc) browser_connection_init,
			0
		};

		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "BrowserConnection", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
browser_connection_class_init (BrowserConnectionClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	browser_connection_signals [BUSY] =
		g_signal_new ("busy",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserConnectionClass, busy),
                              NULL, NULL,
                              _marshal_VOID__BOOLEAN_STRING, G_TYPE_NONE,
                              2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	browser_connection_signals [META_CHANGED] =
		g_signal_new ("meta-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserConnectionClass, meta_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                              1, GDA_TYPE_META_STRUCT);
	browser_connection_signals [FAV_CHANGED] =
		g_signal_new ("favorites-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserConnectionClass, favorites_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);
	browser_connection_signals [TRANSACTION_STATUS_CHANGED] =
		g_signal_new ("transaction-status-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserConnectionClass, transaction_status_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);
	browser_connection_signals [TABLE_COLUMN_PREF_CHANGED] =
		g_signal_new ("table-column-pref-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserConnectionClass, table_column_pref_changed),
                              NULL, NULL,
			      _marshal_VOID__POINTER_POINTER_STRING_STRING, G_TYPE_NONE,
                              4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);

	klass->busy = browser_connection_set_busy_state;
	klass->meta_changed = NULL;
	klass->favorites_changed = NULL;
	klass->transaction_status_changed = NULL;
	klass->table_column_pref_changed = NULL;

	/* Properties */
        object_class->set_property = browser_connection_set_property;
        object_class->get_property = browser_connection_get_property;
	g_object_class_install_property (object_class, PROP_GDA_CNC,
                                         g_param_spec_object ("gda-connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));

	object_class->dispose = browser_connection_dispose;
}

static gboolean
wrapper_ioc_cb (GIOChannel *source, GIOCondition condition, BrowserConnection *bcnc)
{
	GIOStatus status;
	gsize nread;
	GdaThreadNotification notif;

	g_assert (source == bcnc->priv->ioc);
//#define DEBUG_POLLING_SWITCH
#ifdef DEBUG_POLLING_SWITCH
	static guint c = 0;
	c++;
	if (c == 4)
		goto onerror;
#endif
	if (condition & G_IO_IN) {
		status = g_io_channel_read_chars (bcnc->priv->ioc, (gchar*) &notif, sizeof (notif),
						  &nread, NULL);
		if ((status != G_IO_STATUS_NORMAL) || (nread != sizeof (notif)))
			goto onerror;

		switch (notif.type) {
		case GDA_THREAD_NOTIFICATION_JOB:
			check_for_wrapper_result (bcnc);
			break;
		case GDA_THREAD_NOTIFICATION_SIGNAL:
			gda_thread_wrapper_iterate (bcnc->priv->wrapper, FALSE);
			break;
		default:
			/* an error occurred somewhere */
			goto onerror;
		}
	}
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		goto onerror;

	return TRUE; /* keep callback */

 onerror:
#ifdef GDA_DEBUG
	g_print ("Switching to polling instead of notifications...\n");
#endif
	g_source_remove (bcnc->priv->ioc_watch_id);
	bcnc->priv->ioc_watch_id = 0;
	g_io_channel_shutdown (bcnc->priv->ioc, FALSE, NULL);
	g_io_channel_unref (bcnc->priv->ioc);
	bcnc->priv->ioc = NULL;

	setup_results_timer (bcnc);
	return FALSE; /* remove callback */
}

static void
browser_connection_init (BrowserConnection *bcnc)
{
	static guint index = 1;
	bcnc->priv = g_new0 (BrowserConnectionPrivate, 1);
	bcnc->priv->wrapper = gda_thread_wrapper_new ();
	bcnc->priv->ioc = gda_thread_wrapper_get_io_channel (bcnc->priv->wrapper);
	if (bcnc->priv->ioc) {
		g_io_channel_ref (bcnc->priv->ioc);
		bcnc->priv->ioc_watch_id = g_io_add_watch (bcnc->priv->ioc,
							   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
							   (GIOFunc) wrapper_ioc_cb, bcnc);
	}
	else
		bcnc->priv->ioc_watch_id = 0;
	bcnc->priv->wrapper_jobs = NULL;
	bcnc->priv->wrapper_results_timer = 0;
	bcnc->priv->long_timer = FALSE;
	bcnc->priv->nb_no_job_waits = 0;
	bcnc->priv->executed_statements = NULL;

	bcnc->priv->name = g_strdup_printf (_("c%u"), index++);
	bcnc->priv->cnc = NULL;
	bcnc->priv->parser = NULL;
	bcnc->priv->variables = NULL;
	memset (&(bcnc->priv->dsn_info), 0, sizeof (GdaDsnInfo));
#if GLIB_CHECK_VERSION(2,31,7)
	g_mutex_init (&bcnc->priv->mstruct_mutex);
#else
	bcnc->priv->p_mstruct_mutex = g_mutex_new ();
#endif
	bcnc->priv->p_mstruct_list = NULL;
	bcnc->priv->c_mstruct = NULL;
	bcnc->priv->mstruct = NULL;

	bcnc->priv->meta_store_signal = 0;

	bcnc->priv->bfav = NULL;

	bcnc->priv->store_cnc = NULL;

	bcnc->priv->variables = NULL;
	/*g_print ("Created BrowserConnection %p\n", bcnc);*/
}

static void
transaction_status_changed_cb (G_GNUC_UNUSED GdaThreadWrapper *wrapper, G_GNUC_UNUSED gpointer instance,
			       G_GNUC_UNUSED const gchar *signame, G_GNUC_UNUSED gint n_param_values,
			       G_GNUC_UNUSED const GValue *param_values, G_GNUC_UNUSED gpointer gda_reserved,
			       BrowserConnection *bcnc)
{
	g_signal_emit (bcnc, browser_connection_signals [TRANSACTION_STATUS_CHANGED], 0);
}

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_meta_store_update (BrowserConnection *bcnc, GError **error)
{
	gboolean retval;
	GdaMetaContext context = {"_tables", 0, NULL, NULL};
	retval = gda_connection_update_meta_store (bcnc->priv->cnc, &context, error);

	return GINT_TO_POINTER (retval ? 2 : 1);
}

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_meta_struct_sync (BrowserConnection *bcnc, GError **error)
{
	gboolean retval = TRUE;
	GdaMetaStruct *mstruct;

	MUTEX_LOCK (bcnc);
	g_assert (bcnc->priv->p_mstruct_list);
	mstruct = (GdaMetaStruct *) bcnc->priv->p_mstruct_list->data;
	/*g_print ("%s() for GdaMetaStruct %p\n", __FUNCTION__, mstruct);*/
	bcnc->priv->p_mstruct_list = g_slist_delete_link (bcnc->priv->p_mstruct_list,
							  bcnc->priv->p_mstruct_list);
	if (bcnc->priv->p_mstruct_list) {
		/* don't care about this one */
		g_object_unref (G_OBJECT (mstruct));
		MUTEX_UNLOCK (bcnc);
		return GINT_TO_POINTER (3);
	}
	else {
		if (bcnc->priv->c_mstruct)
			g_object_unref (bcnc->priv->c_mstruct);
		bcnc->priv->c_mstruct = mstruct;

		/*g_print ("Meta struct sync for %p\n", mstruct);*/
		retval = gda_meta_struct_complement_all (mstruct, error);
		MUTEX_UNLOCK (bcnc);
	}

#ifdef GDA_DEBUG_NO
	GSList *all, *list;
	g_print ("%s() %p:\n", __FUNCTION__, bcnc->priv->mstruct);
	all = gda_meta_struct_get_all_db_objects (bcnc->priv->mstruct);
	for (list = all; list; list = list->next) {
		GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
		g_print ("DBO, Type %d: short=>[%s] schema=>[%s] full=>[%s]\n", dbo->obj_type,
			 dbo->obj_short_name, dbo->obj_schema, dbo->obj_full_name);
	}
	g_slist_free (all);
#endif

	return GINT_TO_POINTER (retval ? 2 : 1);
}


static void
meta_changed_cb (G_GNUC_UNUSED GdaThreadWrapper *wrapper,
		 G_GNUC_UNUSED GdaMetaStore *store,
		 G_GNUC_UNUSED const gchar *signame,
		 G_GNUC_UNUSED gint n_param_values,
		 G_GNUC_UNUSED const GValue *param_values,
		 G_GNUC_UNUSED gpointer gda_reserved,
		 BrowserConnection *bcnc)
{
	guint job_id;
	GError *lerror = NULL;
	GdaMetaStruct *mstruct;

	MUTEX_LOCK (bcnc);
	mstruct = gda_meta_struct_new (gda_connection_get_meta_store (bcnc->priv->cnc),
				       GDA_META_STRUCT_FEATURE_ALL);
	bcnc->priv->p_mstruct_list = g_slist_append (bcnc->priv->p_mstruct_list, mstruct);
	/*g_print ("%s() Added %p to p_mstruct_list\n", __FUNCTION__, mstruct);*/
	MUTEX_UNLOCK (bcnc);
	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_meta_struct_sync,
					     g_object_ref (bcnc), g_object_unref, &lerror);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STRUCT_SYNC,
				  _("Analysing database schema"), NULL, NULL);
	else if (lerror) {
		browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
				    lerror->message ? lerror->message : _("No detail"));
		g_error_free (lerror);
	}
}

/**
 * browser_connection_meta_data_changed
 * @bcnc: a #BrowserConnection
 *
 * Call this function if the meta data has been changed directly (ie. for example after
 * declaring or undeclaring a foreign key). This call creates a new #GdaMetaStruct internally used.
 */
void
browser_connection_meta_data_changed (BrowserConnection *bcnc)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	meta_changed_cb (NULL, NULL, NULL, 0, NULL, NULL, bcnc);
}

static gpointer
wrapper_have_meta_store_ready (BrowserConnection *bcnc, GError **error)
{
	gchar *dict_file_name = NULL;
	gboolean update_store = FALSE;
	GdaMetaStore *store;
	gchar *cnc_string, *cnc_info;

	g_object_get (G_OBJECT (bcnc->priv->cnc),
		      "dsn", &cnc_info,
		      "cnc-string", &cnc_string, NULL);
	dict_file_name = config_info_compute_dict_file_name (cnc_info ? gda_config_get_dsn_info (cnc_info) : NULL,
							     cnc_string);
	g_free (cnc_string);
	if (dict_file_name) {
		if (BROWSER_IS_VIRTUAL_CONNECTION (bcnc))
			/* force meta store update in case of virtual connection */
			update_store = TRUE;
		else if (! g_file_test (dict_file_name, G_FILE_TEST_EXISTS))
			update_store = TRUE;
		store = gda_meta_store_new_with_file (dict_file_name);
	}
	else {
		store = gda_meta_store_new (NULL);
		if (store)
			update_store = TRUE;
	}
	config_info_update_meta_store_properties (store, bcnc->priv->cnc);

	bcnc->priv->dict_file_name = dict_file_name;
	g_object_set (G_OBJECT (bcnc->priv->cnc), "meta-store", store, NULL);
	if (update_store) {
		gboolean retval;
		GdaMetaContext context = {"_tables", 0, NULL, NULL};
		retval = gda_connection_update_meta_store (bcnc->priv->cnc, &context, error);
		if (!retval) {
			g_object_unref (store);
			return NULL;
		}
	}

	gboolean retval = TRUE;
	GdaMetaStruct *mstruct;
	mstruct = gda_meta_struct_new (store, GDA_META_STRUCT_FEATURE_ALL);
	MUTEX_LOCK (bcnc);
	if (bcnc->priv->c_mstruct) {
		g_object_unref (bcnc->priv->c_mstruct);
		bcnc->priv->c_mstruct = NULL;
	}
	bcnc->priv->mstruct = mstruct;
	retval = gda_meta_struct_complement_all (mstruct, error);
	MUTEX_UNLOCK (bcnc);

#ifdef GDA_DEBUG_NO
	GSList *all, *list;
	g_print ("%s() %p:\n", __FUNCTION__, bcnc->priv->mstruct);
	all = gda_meta_struct_get_all_db_objects (bcnc->priv->mstruct);
	for (list = all; list; list = list->next) {
		GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
		g_print ("DBO, Type %d: short=>[%s] schema=>[%s] full=>[%s]\n", dbo->obj_type,
			 dbo->obj_short_name, dbo->obj_schema, dbo->obj_full_name);
	}
	g_slist_free (all);
#endif
	bcnc->priv->meta_store_signal =
		gda_thread_wrapper_connect_raw (bcnc->priv->wrapper, store, "meta-changed",
						FALSE, FALSE,
						(GdaThreadWrapperCallback) meta_changed_cb,
						bcnc);
	g_object_unref (store);
	return retval ? (void*) 0x01 : NULL;
}

typedef struct {
        guint jid;
        GMainLoop *loop;
        GError **error;
        GdaThreadWrapper *wrapper;

        /* out */
	gboolean retval;
} MainloopData;

static gboolean
check_for_meta_store_ready (MainloopData *data)
{
	gpointer retval;
        GError *lerror = NULL;

        retval = gda_thread_wrapper_fetch_result (data->wrapper, FALSE, data->jid, &lerror);
        if (retval || (!retval && lerror)) {
                /* waiting is finished! */
                data->retval = retval ? TRUE : FALSE;
                if (lerror)
                        g_propagate_error (data->error, lerror);
                g_main_loop_quit (data->loop);
                return FALSE;
        }
        return TRUE;
}


static void
browser_connection_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
        BrowserConnection *bcnc;

        bcnc = BROWSER_CONNECTION (object);
        if (bcnc->priv) {
                switch (param_id) {
                case PROP_GDA_CNC:
                        bcnc->priv->cnc = (GdaConnection*) g_value_get_object (value);
                        if (!bcnc->priv->cnc)
				return;

			/*g_print ("BrowserConnection %p [%s], wrapper %p, GdaConnection %p\n",
			  bcnc, bcnc->priv->name, bcnc->priv->wrapper, bcnc->priv->cnc);*/
			g_object_ref (bcnc->priv->cnc);
			g_object_set (G_OBJECT (bcnc->priv->cnc), "execution-timer", TRUE, NULL);
			bcnc->priv->transaction_status_signal =
				gda_thread_wrapper_connect_raw (bcnc->priv->wrapper,
								bcnc->priv->cnc,
								"transaction-status-changed",
								FALSE, FALSE,
								(GdaThreadWrapperCallback) transaction_status_changed_cb,
								bcnc);


			/* meta store, open it in a sub thread to avoid locking the GTK thread */
			GError *lerror = NULL;
			guint jid;
			jid = gda_thread_wrapper_execute (bcnc->priv->wrapper,
							  (GdaThreadWrapperFunc) wrapper_have_meta_store_ready,
							  (gpointer) bcnc,
							  NULL, &lerror);
			if (jid == 0) {
				browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
						    lerror->message ? lerror->message : _("No detail"));
				g_clear_error (&lerror);
			}
			else {
				GMainLoop *loop;
				MainloopData data;

				loop = g_main_loop_new (NULL, FALSE);
				data.jid = jid;
				data.loop = loop;
				data.error = &lerror;
				data.wrapper = bcnc->priv->wrapper;
				g_timeout_add (200, (GSourceFunc) check_for_meta_store_ready, &data);
				g_main_loop_run (loop);
				g_main_loop_unref (loop);
				if (!data.retval) {
					browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
							    lerror->message ? lerror->message : _("No detail"));
					g_clear_error (&lerror);
				}
			}
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}



static void
browser_connection_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
        BrowserConnection *bcnc;

        bcnc = BROWSER_CONNECTION (object);
        if (bcnc->priv) {
                switch (param_id) {
                case PROP_GDA_CNC:
                        g_value_set_object (value, bcnc->priv->cnc);
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
clear_dsn_info (BrowserConnection *bcnc)
{
        g_free (bcnc->priv->dsn_info.name);
        bcnc->priv->dsn_info.name = NULL;

        g_free (bcnc->priv->dsn_info.provider);
        bcnc->priv->dsn_info.provider = NULL;

        g_free (bcnc->priv->dsn_info.description);
        bcnc->priv->dsn_info.description = NULL;

        g_free (bcnc->priv->dsn_info.cnc_string);
        bcnc->priv->dsn_info.cnc_string = NULL;

        g_free (bcnc->priv->dsn_info.auth_string);
        bcnc->priv->dsn_info.auth_string = NULL;
}

static void
fav_changed_cb (G_GNUC_UNUSED ToolsFavorites *bfav, BrowserConnection *bcnc)
{
	g_signal_emit (bcnc, browser_connection_signals [FAV_CHANGED], 0);
}

static void
browser_connection_dispose (GObject *object)
{
	BrowserConnection *bcnc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_CONNECTION (object));

	bcnc = BROWSER_CONNECTION (object);
	if (bcnc->priv) {
		if (bcnc->priv->results_timer_id) {
			g_source_remove (bcnc->priv->results_timer_id);
			bcnc->priv->results_timer_id = 0;
		}
		if (bcnc->priv->results_list) {
			g_slist_foreach (bcnc->priv->results_list, (GFunc) g_free, NULL);
			g_slist_free (bcnc->priv->results_list);
			bcnc->priv->results_list = NULL;
		}

		if (bcnc->priv->variables)
			g_object_unref (bcnc->priv->variables);

		if (bcnc->priv->store_cnc)
			g_object_unref (bcnc->priv->store_cnc);

		if (bcnc->priv->executed_statements)
			g_hash_table_destroy (bcnc->priv->executed_statements);

		clear_dsn_info (bcnc);

		g_free (bcnc->priv->dict_file_name);

		if (bcnc->priv->wrapper_jobs) {
			g_slist_foreach (bcnc->priv->wrapper_jobs, (GFunc) wrapper_job_free, NULL);
			g_slist_free (bcnc->priv->wrapper_jobs);
		}

		if (bcnc->priv->wrapper_results_timer > 0)
			g_source_remove (bcnc->priv->wrapper_results_timer);

		if (bcnc->priv->meta_store_signal)
			gda_thread_wrapper_disconnect (bcnc->priv->wrapper,
						       bcnc->priv->meta_store_signal);
		if (bcnc->priv->transaction_status_signal)
			gda_thread_wrapper_disconnect (bcnc->priv->wrapper,
						       bcnc->priv->transaction_status_signal);

		g_object_unref (bcnc->priv->wrapper);
		g_free (bcnc->priv->name);
		if (bcnc->priv->c_mstruct)
			g_object_unref (bcnc->priv->c_mstruct);
		if (bcnc->priv->mstruct)
			g_object_unref (bcnc->priv->mstruct);
		if (bcnc->priv->p_mstruct_list) {
			g_slist_foreach (bcnc->priv->p_mstruct_list, (GFunc) g_object_unref, NULL);
			g_slist_free (bcnc->priv->p_mstruct_list);
		}
#if GLIB_CHECK_VERSION(2,31,7)
		g_mutex_clear (&bcnc->priv->mstruct_mutex);
#else
		if (bcnc->priv->p_mstruct_mutex)
			g_mutex_free (bcnc->priv->p_mstruct_mutex);
#endif

		if (bcnc->priv->cnc)
			g_object_unref (bcnc->priv->cnc);

		if (bcnc->priv->parser)
			g_object_unref (bcnc->priv->parser);
		if (bcnc->priv->bfav) {
			g_signal_handlers_disconnect_by_func (bcnc->priv->bfav,
							      G_CALLBACK (fav_changed_cb), bcnc);
			g_object_unref (bcnc->priv->bfav);
		}
		browser_connection_set_busy_state (bcnc, FALSE, NULL);

		if (bcnc->priv->ioc_watch_id > 0) {
			g_source_remove (bcnc->priv->ioc_watch_id);
			bcnc->priv->ioc_watch_id = 0;
		}

		if (bcnc->priv->ioc) {
			g_io_channel_unref (bcnc->priv->ioc);
			bcnc->priv->ioc = NULL;
		}

		g_free (bcnc->priv);
		bcnc->priv = NULL;
		/*g_print ("Disposed BrowserConnection %p\n", bcnc);*/
	}

	/* parent class */
	parent_class->dispose (object);
}

static gboolean
check_for_wrapper_result (BrowserConnection *bcnc)
{
	GError *lerror = NULL;
	gpointer exec_res = NULL;
	WrapperJob *wj;
	gboolean retval = TRUE; /* return FALSE to interrupt current timer */

	retval = !setup_results_timer (bcnc);
	if (!bcnc->priv->wrapper_jobs) {
		gda_thread_wrapper_iterate (bcnc->priv->wrapper, FALSE);
		return retval;
	}

	wj = (WrapperJob*) bcnc->priv->wrapper_jobs->data;
	exec_res = gda_thread_wrapper_fetch_result (bcnc->priv->wrapper,
						    FALSE, 
						    wj->job_id, &lerror);
	if (exec_res) {
		switch (wj->job_type) {
		case JOB_TYPE_META_STORE_UPDATE: {
			if (GPOINTER_TO_INT (exec_res) == 1) {
				browser_show_error (NULL, _("Error while analysing database schema: %s"),
						    lerror && lerror->message ? lerror->message : _("No detail"));
				g_clear_error (&lerror);
			}
			else if (! bcnc->priv->meta_store_signal) {
				GdaMetaStore *store;
				store = gda_connection_get_meta_store (bcnc->priv->cnc);
				meta_changed_cb (bcnc->priv->wrapper, store,
						 NULL, 0, NULL, NULL, bcnc);
				bcnc->priv->meta_store_signal =
					gda_thread_wrapper_connect_raw (bcnc->priv->wrapper, store, "meta-changed",
									FALSE, FALSE,
									(GdaThreadWrapperCallback) meta_changed_cb,
									bcnc);

			}
			break;
		}
		case JOB_TYPE_META_STRUCT_SYNC: {
			if (GPOINTER_TO_INT (exec_res) == 1) {
				browser_show_error (NULL, _("Error while analysing database schema: %s"),
						    lerror && lerror->message ? lerror->message : _("No detail"));
				g_clear_error (&lerror);
			}
			else if (GPOINTER_TO_INT (exec_res) == 3) {
				/* nothing to do */
			}
			else {
				MUTEX_LOCK (bcnc);
				
				if (bcnc->priv->c_mstruct) {
					GdaMetaStruct *old_mstruct;
					old_mstruct = bcnc->priv->mstruct;
					bcnc->priv->mstruct = bcnc->priv->c_mstruct;
					bcnc->priv->c_mstruct = NULL;
					if (old_mstruct)
						g_object_unref (old_mstruct);
#ifdef GDA_DEBUG_NO
					GSList *all, *list;
					g_print ("Signalling change for GdaMetaStruct %p:\n", bcnc->priv->mstruct);
					all = gda_meta_struct_get_all_db_objects (bcnc->priv->mstruct);
					for (list = all; list; list = list->next) {
						GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
						g_print ("DBO, Type %d: short=>[%s] schema=>[%s] full=>[%s]\n", dbo->obj_type,
							 dbo->obj_short_name, dbo->obj_schema, dbo->obj_full_name);
					}
					g_slist_free (all);
#endif
					g_signal_emit (bcnc, browser_connection_signals [META_CHANGED], 0, bcnc->priv->mstruct);
				}
				MUTEX_UNLOCK (bcnc);
			}
			break;
		}
		case JOB_TYPE_STATEMENT_EXECUTE: {
			guint *id;
			StatementResult *res;

			if (! bcnc->priv->executed_statements)
				bcnc->priv->executed_statements = g_hash_table_new_full (g_int_hash, g_int_equal,
											 g_free,
											 (GDestroyNotify) statement_result_free);
			id = g_new (guint, 1);
			*id = wj->job_id;
			res = g_new0 (StatementResult, 1);
			if (exec_res == (gpointer) 0x01)
				res->error = lerror;
			else {
				res->result = G_OBJECT (exec_res);
				res->last_inserted_row = g_object_get_data (exec_res, "__bcnc_last_inserted_row");
				if (res->last_inserted_row)
					g_object_set_data (exec_res, "__bcnc_last_inserted_row", NULL);
			}
			g_hash_table_insert (bcnc->priv->executed_statements, id, res);
			break;
		}
		case JOB_TYPE_CALLBACK:
			if (wj->callback) {
				wj->callback (bcnc, exec_res == (gpointer) 0x01 ? NULL : exec_res,
					      wj->cb_data, lerror);
				g_clear_error (&lerror);
			}
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		pop_wrapper_job (bcnc, wj);
	}

	if (bcnc->priv->wrapper_jobs) {
		wj = (WrapperJob*) bcnc->priv->wrapper_jobs->data;
		if (exec_res)
			g_signal_emit (bcnc, browser_connection_signals [BUSY], 0, TRUE, wj->reason);
	}
	return retval;
}

/**
 * browser_connection_new
 * @cnc: a #GdaConnection
 *
 * Creates a new #BrowserConnection object wrapping @cnc. The browser_core_take_connection() method
 * must be called on the new object to mahe it managed by the browser.
 *
 * To close the new connection, use browser_core_close_connection().
 *
 * Returns: a new object
 */
BrowserConnection*
browser_connection_new (GdaConnection *cnc)
{
	BrowserConnection *bcnc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	bcnc = BROWSER_CONNECTION (g_object_new (BROWSER_TYPE_CONNECTION, "gda-connection", cnc, NULL));

	return bcnc;
}

/**
 * browser_connection_get_name
 * @bcnc: a #BrowserConnection
 *
 * Returns: @bcnc's name
 */
const gchar *
browser_connection_get_name (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return bcnc->priv->name;
}

/**
 * browser_connection_get_long_name:
 * @bcnc: a #BrowserConnection
 *
 * Get the "long" name of @bcnc
 *
 * Returns: a new string
 */
gchar *
browser_connection_get_long_name (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	const gchar *cncname;
	const GdaDsnInfo *dsn;
	GString *title;

	dsn = browser_connection_get_information (bcnc);
	cncname = browser_connection_get_name (bcnc);
	title = g_string_new (_("Connection"));
	g_string_append (title, " ");
	g_string_append_printf (title, "'%s'", cncname ? cncname : _("unnamed"));
	if (dsn) {
		if (dsn->name)
			g_string_append_printf (title, ", %s '%s'", _("data source"), dsn->name);
		if (dsn->provider)
			g_string_append_printf (title, " (%s)", dsn->provider);
	}
	return g_string_free (title, FALSE);
}

/**
 * browser_connection_get_information
 * @bcnc: a #BrowserConnection
 *
 * Get some information about the connection
 *
 * Returns: a pointer to the associated #GdaDsnInfo
 */
const GdaDsnInfo *
browser_connection_get_information (BrowserConnection *bcnc)
{
	gboolean is_wrapper;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	clear_dsn_info (bcnc);
	if (!bcnc->priv->cnc)
		return NULL;
	
	g_object_get (G_OBJECT (bcnc->priv->cnc), "is-wrapper", &is_wrapper, NULL);

	if (!is_wrapper && gda_connection_get_provider_name (bcnc->priv->cnc))
		bcnc->priv->dsn_info.provider = g_strdup (gda_connection_get_provider_name (bcnc->priv->cnc));
	if (gda_connection_get_dsn (bcnc->priv->cnc)) {
		bcnc->priv->dsn_info.name = g_strdup (gda_connection_get_dsn (bcnc->priv->cnc));
		if (! bcnc->priv->dsn_info.provider) {
			GdaDsnInfo *cinfo;
			cinfo = gda_config_get_dsn_info (bcnc->priv->dsn_info.name);
			if (cinfo && cinfo->provider)
				bcnc->priv->dsn_info.provider = g_strdup (cinfo->provider);
		}
	}
	if (gda_connection_get_cnc_string (bcnc->priv->cnc))
		bcnc->priv->dsn_info.cnc_string = g_strdup (gda_connection_get_cnc_string (bcnc->priv->cnc));
	if (is_wrapper && bcnc->priv->dsn_info.cnc_string) {
		GdaQuarkList *ql;
		const gchar *prov;
		ql = gda_quark_list_new_from_string (bcnc->priv->dsn_info.cnc_string);
		prov = gda_quark_list_find (ql, "PROVIDER_NAME");
		if (prov)
			bcnc->priv->dsn_info.provider = g_strdup (prov);
		gda_quark_list_free (ql);
	}
	if (gda_connection_get_authentication (bcnc->priv->cnc))
		bcnc->priv->dsn_info.auth_string = g_strdup (gda_connection_get_authentication (bcnc->priv->cnc));

	return &(bcnc->priv->dsn_info);
}

/**
 * browser_connection_is_virtual
 * @bcnc: a #BrowserConnection
 *
 * Tells if @bcnc is a virtual connection or not
 *
 * Returns: %TRUE if @bcnc is a virtual connection
 */
gboolean
browser_connection_is_virtual (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	if (GDA_IS_VIRTUAL_CONNECTION (bcnc->priv->cnc))
		return TRUE;
	else
		return FALSE;
}

/**
 * browser_connection_is_busy
 * @bcnc: a #BrowserConnection
 * @out_reason: a pointer to store a copy of the reason @bcnc is busy (will be set 
 *              to %NULL if @bcnc is not busy), or %NULL
 *
 * Tells if @bcnc is currently busy or not.
 *
 * Returns: %TRUE if @bcnc is busy
 */
gboolean
browser_connection_is_busy (BrowserConnection *bcnc, gchar **out_reason)
{
	if (out_reason)
		*out_reason = NULL;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);

	if (out_reason && bcnc->priv->busy_reason)
		*out_reason = g_strdup (bcnc->priv->busy_reason);

	return bcnc->priv->busy;
}

/**
 * browser_connection_update_meta_data
 * @bcnc: a #BrowserConnection
 *
 * Make @bcnc update its meta store in the background.
 */
void
browser_connection_update_meta_data (BrowserConnection *bcnc)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));

	if (bcnc->priv->wrapper_jobs) {
		WrapperJob *wj;
		wj = (WrapperJob*) g_slist_last (bcnc->priv->wrapper_jobs)->data;
		if (wj->job_type == JOB_TYPE_META_STORE_UPDATE) {
			/* nothing to do */
			return;
		}
	}

	if (bcnc->priv->meta_store_signal) {
		gda_thread_wrapper_disconnect (bcnc->priv->wrapper,
					       bcnc->priv->meta_store_signal);
		bcnc->priv->meta_store_signal = 0;
	}

	guint job_id;
	GError *lerror = NULL;
	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_meta_store_update,
					     g_object_ref (bcnc), g_object_unref, &lerror);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STORE_UPDATE,
				  _("Getting database schema information"), NULL, NULL);
	else if (lerror) {
		browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
				    lerror->message ? lerror->message : _("No detail"));
		g_error_free (lerror);
	}
}

/**
 * browser_connection_get_meta_struct
 * @bcnc: a #BrowserConnection
 *
 * Get the #GdaMetaStruct maintained up to date by @bcnc.
 *
 * Returns: a #GdaMetaStruct, the caller does not have any reference to it.
 */
GdaMetaStruct *
browser_connection_get_meta_struct (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return bcnc->priv->mstruct;
}

/**
 * browser_connection_get_meta_store
 * @bcnc: a #BrowserConnection
 *
 * Returns: @bcnc's #GdaMetaStore, the caller does not have any reference to it.
 */
GdaMetaStore *
browser_connection_get_meta_store (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return gda_connection_get_meta_store (bcnc->priv->cnc);
}

/**
 * browser_connection_get_dictionary_file
 * @bcnc: a #BrowserConnection
 *
 * Returns: the dictionary file name used by @bcnc, or %NULL
 */
const gchar *
browser_connection_get_dictionary_file (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return bcnc->priv->dict_file_name;
}

/**
 * browser_connection_get_transaction_status
 * @bcnc: a #BrowserConnection
 *
 * Retuns: the #GdaTransactionStatus of the connection, or %NULL
 */
GdaTransactionStatus *
browser_connection_get_transaction_status (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return gda_connection_get_transaction_status (bcnc->priv->cnc);
}

/**
 * browser_connection_begin
 * @bcnc: a #BrowserConnection
 * @error: a place to store errors, or %NULL
 *
 * Begins a transaction
 */
gboolean
browser_connection_begin (BrowserConnection *bcnc, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	return gda_connection_begin_transaction (bcnc->priv->cnc, NULL,
						 GDA_TRANSACTION_ISOLATION_UNKNOWN, error);
}

/**
 * browser_connection_commit
 * @bcnc: a #BrowserConnection
 * @error: a place to store errors, or %NULL
 *
 * Commits a transaction
 */
gboolean
browser_connection_commit (BrowserConnection *bcnc, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	return gda_connection_commit_transaction (bcnc->priv->cnc, NULL, error);
}

/**
 * browser_connection_rollback
 * @bcnc: a #BrowserConnection
 * @error: a place to store errors, or %NULL
 *
 * Rolls back a transaction
 */
gboolean
browser_connection_rollback (BrowserConnection *bcnc, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	return gda_connection_rollback_transaction (bcnc->priv->cnc, NULL, error);
}

/**
 * browser_connection_get_favorites
 * @bcnc: a #BrowserConnection
 *
 * Get @bcnc's favorites handler
 *
 * Returns: (transfer none): the #ToolsFavorites used by @bcnc
 */
ToolsFavorites *
browser_connection_get_favorites (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	if (!bcnc->priv->bfav && !BROWSER_IS_VIRTUAL_CONNECTION (bcnc)) {
		bcnc->priv->bfav = tools_favorites_new (gda_connection_get_meta_store (bcnc->priv->cnc));
		g_signal_connect (bcnc->priv->bfav, "favorites-changed",
				  G_CALLBACK (fav_changed_cb), bcnc);
	}
	return bcnc->priv->bfav;
}

/**
 * browser_connection_get_completions
 * @bcnc: a #BrowserConnection
 * @sql:
 * @start:
 * @end:
 *
 * See gda_completion_list_get()
 *
 * Returns: a new array of strings, or NULL (use g_strfreev() to free the returned array)
 */
gchar **
browser_connection_get_completions (BrowserConnection *bcnc, const gchar *sql,
				    gint start, gint end)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return gda_completion_list_get (bcnc->priv->cnc, sql, start, end);
}


/**
 * browser_connection_create_parser
 * @bcnc: a #BrowserConnection
 *
 * Get a new #GdaSqlParser object for @bcnc
 *
 * Returns: a new #GdaSqlParser
 */
GdaSqlParser *
browser_connection_create_parser (BrowserConnection *bcnc)
{
	GdaSqlParser *parser;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	
	parser = gda_connection_create_parser (bcnc->priv->cnc);
	if (!parser)
		parser = gda_sql_parser_new ();
	return parser;
}

/**
 * browser_connection_render_pretty_sql
 * @bcnc: a #BrowserConnection
 * @stmt: a #GdaStatement
 *
 * Renders @stmt as SQL well indented
 *
 * Returns: a new string
 */
gchar *
browser_connection_render_pretty_sql (BrowserConnection *bcnc, GdaStatement *stmt)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	return gda_statement_to_sql_extended (stmt, bcnc->priv->cnc, NULL,
					      GDA_STATEMENT_SQL_PRETTY |
					      GDA_STATEMENT_SQL_PARAMS_SHORT,
					      NULL, NULL);
}

typedef struct {
	GdaConnection *cnc;
	GdaStatement *stmt;
	GdaSet *params;
	GdaStatementModelUsage model_usage;
	gboolean need_last_insert_row;
} StmtExecData;

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_statement_execute (StmtExecData *data, GError **error)
{
	GObject *obj;
	GdaSet *last_insert_row = NULL;
	GError *lerror = NULL;
	obj = gda_connection_statement_execute (data->cnc, data->stmt,
						data->params, data->model_usage,
						data->need_last_insert_row ? &last_insert_row : NULL,
						&lerror);
	if (obj) {
		if (GDA_IS_DATA_MODEL (obj))
			/* force loading of rows if necessary */
			gda_data_model_get_n_rows ((GdaDataModel*) obj);
		else if (last_insert_row)
			g_object_set_data (obj, "__bcnc_last_inserted_row", last_insert_row);
	}
	else {
		if (lerror)
			g_propagate_error (error, lerror);
		else {
			g_warning (_("Execution reported an undefined error, please report error to "
				     "http://bugzilla.gnome.org/ for the \"libgda\" product"));
			g_set_error (error, TOOLS_ERROR, TOOLS_INTERNAL_COMMAND_ERROR,
				     "%s", _("No detail"));
		}
	}
	return obj ? obj : (gpointer) 0x01;
}

/**
 * browser_connection_execute_statement
 * @bcnc: a #BrowserConnection
 * @stmt: a #GdaStatement
 * @params: a #GdaSet as parameters, or %NULL
 * @model_usage: how the returned data model (if any) will be used
 * @need_last_insert_row: %TRUE if the values of the last interted row must be computed
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt by @bcnc. Unless specific requirements, it's easier to use
 * browser_connection_execute_statement_cb().
 *
 * Returns: a job ID, to be used with browser_connection_execution_get_result(), or %0 if an
 * error occurred
 */
guint
browser_connection_execute_statement (BrowserConnection *bcnc,
				      GdaStatement *stmt,
				      GdaSet *params,
				      GdaStatementModelUsage model_usage,
				      gboolean need_last_insert_row,
				      GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), 0);
	g_return_val_if_fail (!params || GDA_IS_SET (params), 0);

	StmtExecData *data;
	guint job_id;

	data = g_new0 (StmtExecData, 1);
	data->cnc = bcnc->priv->cnc;
	data->stmt = stmt;
	data->params = params;
	data->model_usage = model_usage;
	data->need_last_insert_row = need_last_insert_row;

	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_statement_execute,
					     data, (GDestroyNotify) g_free, error);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_STATEMENT_EXECUTE,
				  _("Executing a query"), NULL, NULL);

	return job_id;
}

typedef struct {
	GdaConnection *cnc;
	GdaDataModel *model;
} RerunSelectData;

/* executed in @bcnc->priv->wrapper's sub thread */
static gpointer
wrapper_rerun_select (RerunSelectData *data, GError **error)
{
	gboolean retval;

	retval = gda_data_select_rerun (GDA_DATA_SELECT (data->model), error);
	return retval ? data->model : (gpointer) 0x01;
}

/**
 * browser_connection_rerun_select
 * @bcnc: a #BrowserConnection object
 * @model: a #GdaDataModel, which has to ba a #GdaDataSelect
 * @error: a place to store errors, or %NULL
 *
 * Re-execute @model
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_rerun_select (BrowserConnection *bcnc,
				 GdaDataModel *model,
				 GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);

	RerunSelectData *data;
	guint job_id;

	data = g_new0 (RerunSelectData, 1);
	data->cnc = bcnc->priv->cnc;
	data->model = model;

	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_rerun_select,
					     data, (GDestroyNotify) g_free, error);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_STATEMENT_EXECUTE,
				  _("Executing a query"), NULL, NULL);

	return job_id;
}


/**
 * browser_connection_execution_get_result
 * @bcnc: a #BrowserConnection
 * @exec_id: the ID of the excution
 * @last_insert_row: a place to store the last inserted row, if any, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Pick up the result of the @exec_id's execution.
 *
 * Returns: the execution result, or %NULL if either an error occurred or the result is not yet ready
 */
GObject *
browser_connection_execution_get_result (BrowserConnection *bcnc, guint exec_id,
					 GdaSet **last_insert_row, GError **error)
{
	StatementResult *res;
	guint id;
	GObject *retval;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (exec_id > 0, NULL);

	if (! bcnc->priv->executed_statements)
		return NULL;

	id = exec_id;
	res = g_hash_table_lookup (bcnc->priv->executed_statements, &id);
	if (!res)
		return NULL;

	retval = res->result;
	res->result = NULL;

	if (last_insert_row) {
		*last_insert_row = res->last_inserted_row;
		res->last_inserted_row = NULL;
	}
	
	if (res->error) {
		g_propagate_error (error, res->error);
		res->error = NULL;
	}

	g_hash_table_remove (bcnc->priv->executed_statements, &id);
	/*if (GDA_IS_DATA_MODEL (retval))
	  gda_data_model_dump (GDA_DATA_MODEL (retval), NULL);*/
	return retval;
}

/**
 * browser_connection_job_cancel:
 * @bcnc: a #BrowserConnection
 * @job_id: the job_id to cancel
 *
 * Cancel a job, from the job ID returned by browser_connection_ldap_describe_entry()
 * or browser_connection_ldap_get_entry_children().
 */
void
browser_connection_job_cancel (BrowserConnection *bcnc, guint job_id)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	g_return_if_fail (job_id > 0);

	TO_IMPLEMENT;
}

static gboolean query_exec_fetch_cb (BrowserConnection *bcnc);

typedef struct {
	guint exec_id;
	gboolean need_last_insert_row;
	BrowserConnectionExecuteCallback callback;
	gpointer cb_data;
} ExecCallbackData;

/**
 * browser_connection_execute_statement_cb
 * @bcnc: a #BrowserConnection
 * @stmt: a #GdaStatement
 * @params: a #GdaSet as parameters, or %NULL
 * @model_usage: how the returned data model (if any) will be used
 * @need_last_insert_row: %TRUE if the values of the last interted row must be computed
 * @callback: the function to call when statement has been executed
 * @data: data to pass to @callback, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt by @bcnc and calls @callback when done. This occurs in the UI thread and avoids
 * having to set up a waiting mechanism to call browser_connection_execution_get_result()
 * repeatedly.
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_execute_statement_cb (BrowserConnection *bcnc,
					 GdaStatement *stmt,
					 GdaSet *params,
					 GdaStatementModelUsage model_usage,
					 gboolean need_last_insert_row,
					 BrowserConnectionExecuteCallback callback,
					 gpointer data,
					 GError **error)
{
	guint exec_id;
	g_return_val_if_fail (callback, 0);

	exec_id = browser_connection_execute_statement (bcnc, stmt, params, model_usage,
							need_last_insert_row, error);
	if (!exec_id)
		return 0;
	ExecCallbackData *cbdata;
	cbdata = g_new0 (ExecCallbackData, 1);
	cbdata->exec_id = exec_id;
	cbdata->need_last_insert_row = need_last_insert_row;
	cbdata->callback = callback;
	cbdata->cb_data = data;

	bcnc->priv->results_list = g_slist_append (bcnc->priv->results_list, cbdata);
	if (! bcnc->priv->results_timer_id)
		bcnc->priv->results_timer_id = g_timeout_add (200,
							      (GSourceFunc) query_exec_fetch_cb,
							      bcnc);
	return exec_id;
}

/**
 * browser_connection_rerun_select_cb
 * @bcnc: a #BrowserConnection object
 * @model: a #GdaDataModel, which has to ba a #GdaDataSelect
 * @callback: the function to call when statement has been executed
 * @data: data to pass to @callback, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Re-execute @model.
 *
 * Warning: gda_data_model_freeze() and gda_data_model_thaw() should be used
 * before and after this call since the model will signal its changes in a thread
 * which is not the GUI thread.
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_rerun_select_cb (BrowserConnection *bcnc,
				    GdaDataModel *model,
				    BrowserConnectionExecuteCallback callback,
				    gpointer data,
				    GError **error)
{
	guint exec_id;
	g_return_val_if_fail (callback, 0);

	exec_id = browser_connection_rerun_select (bcnc, model, error);
	if (!exec_id)
		return 0;
	ExecCallbackData *cbdata;
	cbdata = g_new0 (ExecCallbackData, 1);
	cbdata->exec_id = exec_id;
	cbdata->need_last_insert_row = FALSE;
	cbdata->callback = callback;
	cbdata->cb_data = data;

	bcnc->priv->results_list = g_slist_append (bcnc->priv->results_list, cbdata);
	if (! bcnc->priv->results_timer_id)
		bcnc->priv->results_timer_id = g_timeout_add (200,
							      (GSourceFunc) query_exec_fetch_cb,
							      bcnc);
	return exec_id;
}


static gboolean
query_exec_fetch_cb (BrowserConnection *bcnc)
{
	GObject *res;
	GError *lerror = NULL;
	ExecCallbackData *cbdata;
	GdaSet *last_inserted_row = NULL;

	if (!bcnc->priv->results_list)
		goto out;

	cbdata = (ExecCallbackData *) bcnc->priv->results_list->data;

	if (cbdata->need_last_insert_row)
		res = browser_connection_execution_get_result (bcnc,
							       cbdata->exec_id,
							       &last_inserted_row,
							       &lerror);
	else
		res = browser_connection_execution_get_result (bcnc,
							       cbdata->exec_id, NULL,
							       &lerror);

	if (res || lerror) {
		cbdata->callback (bcnc, cbdata->exec_id, res, last_inserted_row, lerror, cbdata->cb_data);
		if (res)
			g_object_unref (res);
		if (last_inserted_row)
			g_object_unref (last_inserted_row);
		g_clear_error (&lerror);

		bcnc->priv->results_list = g_slist_remove (bcnc->priv->results_list, cbdata);
		g_free (cbdata);
	}

 out:
	if (! bcnc->priv->results_list) {
		bcnc->priv->results_timer_id = 0;
		return FALSE;
	}
	else
		return TRUE; /* keep timer */
}


/**
 * browser_connection_normalize_sql_statement
 * @bcnc: a #BrowserConnection
 * @sqlst: a #GdaSqlStatement
 * @error: a place to store errors, or %NULL
 *
 * See gda_sql_statement_normalize().
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
browser_connection_normalize_sql_statement (BrowserConnection *bcnc,
					    GdaSqlStatement *sqlst, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	
	return gda_sql_statement_normalize (sqlst, bcnc->priv->cnc, error);
}

/**
 * browser_connection_check_sql_statement_validify
 */
gboolean
browser_connection_check_sql_statement_validify (BrowserConnection *bcnc,
						 GdaSqlStatement *sqlst, GError **error)
{
	g_return_val_if_fail (sqlst, FALSE);
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);

	/* check the structure first */
        if (!gda_sql_statement_check_structure (sqlst, error))
                return FALSE;

	return gda_sql_statement_check_validity_m (sqlst, bcnc->priv->mstruct, error);
}



/*
 * DOES NOT emit any signal
 */
void
browser_connection_set_busy_state (BrowserConnection *bcnc, gboolean busy, const gchar *busy_reason)
{
	if (bcnc->priv->busy_reason) {
		g_free (bcnc->priv->busy_reason);
		bcnc->priv->busy_reason = NULL;
	}

	bcnc->priv->busy = busy;
	if (busy_reason)
		bcnc->priv->busy_reason = g_strdup (busy_reason);
}

/*
 *
 * Preferences
 *
 */
#define DBTABLE_PREFERENCES_TABLE_NAME "gda_sql_dbtable_preferences"
#define DBTABLE_PREFERENCES_TABLE_DESC \
        "<table name=\"" DBTABLE_PREFERENCES_TABLE_NAME "\"> "                            \
        "   <column name=\"table_schema\" pkey=\"TRUE\"/>"             \
        "   <column name=\"table_name\" pkey=\"TRUE\"/>"                              \
        "   <column name=\"table_column\" nullok=\"TRUE\" pkey=\"TRUE\"/>"                              \
        "   <column name=\"att_name\"/>"                          \
        "   <column name=\"att_value\"/>"                           \
        "</table>"

static gboolean
meta_store_addons_init (BrowserConnection *bcnc, GError **error)
{
	GError *lerror = NULL;
	GdaMetaStore *store;

	if (!bcnc->priv->cnc) {
		g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Connection not yet opened"));
		return FALSE;
	}
	store = gda_connection_get_meta_store (bcnc->priv->cnc);
	if (!gda_meta_store_schema_add_custom_object (store, DBTABLE_PREFERENCES_TABLE_DESC, &lerror)) {
                g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Can't initialize dictionary to store table preferences"));
		g_warning ("Can't initialize dictionary to store dbtable_preferences :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }

	bcnc->priv->store_cnc = g_object_ref (gda_meta_store_get_internal_connection (store));
	return TRUE;
}


/**
 * browser_connection_set_table_column_attribute
 * @bcnc:
 * @dbo:
 * @column:
 * @attr_name: attribute name, not %NULL
 * @value: value to set, or %NULL to unset
 * @error:
 *
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
browser_connection_set_table_column_attribute (BrowserConnection *bcnc,
					       GdaMetaTable *table,
					       GdaMetaTableColumn *column,
					       const gchar *attr_name,
					       const gchar *value, GError **error)
{
	GdaConnection *store_cnc;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (column, FALSE);
	g_return_val_if_fail (attr_name, FALSE);

	if (! bcnc->priv->store_cnc &&
	    ! meta_store_addons_init (bcnc, error))
		return FALSE;

	store_cnc = bcnc->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (store_cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
                return FALSE;
	}

	/* delete existing attribute */
	GdaStatement *stmt;
	GdaSqlBuilder *builder;
	GdaSet *params;
	GdaSqlBuilderId op_ids[4];
	GdaMetaDbObject *dbo = (GdaMetaDbObject *) table;

	params = gda_set_new_inline (5, "schema", G_TYPE_STRING, dbo->obj_schema,
				     "name", G_TYPE_STRING, dbo->obj_name,
				     "column", G_TYPE_STRING, column->column_name,
				     "attname", G_TYPE_STRING, attr_name,
				     "attvalue", G_TYPE_STRING, value);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (builder, DBTABLE_PREFERENCES_TABLE_NAME);
	op_ids[0] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_schema"),
					      gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[1] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_name"),
					      gda_sql_builder_add_param (builder, "name", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[2] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_column"),
					      gda_sql_builder_add_param (builder, "column", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[3] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "att_name"),
					      gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING,
									 FALSE), 0);
	gda_sql_builder_set_where (builder,
				   gda_sql_builder_add_cond_v (builder, GDA_SQL_OPERATOR_TYPE_AND,
							       op_ids, 4));
	stmt = gda_sql_builder_get_statement (builder, error);
	g_object_unref (G_OBJECT (builder));
	if (!stmt)
		goto err;
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto err;
	}
	g_object_unref (stmt);		

	/* insert new attribute if necessary */
	if (value) {
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
		gda_sql_builder_set_table (builder, DBTABLE_PREFERENCES_TABLE_NAME);
		gda_sql_builder_add_field_value_id (builder,
					      gda_sql_builder_add_id (builder, "table_schema"),
					      gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
					      gda_sql_builder_add_id (builder, "table_name"),
					      gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
					      gda_sql_builder_add_id (builder, "table_column"),
					      gda_sql_builder_add_param (builder, "column", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
					      gda_sql_builder_add_id (builder, "att_name"),
					      gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
					      gda_sql_builder_add_id (builder, "att_value"),
					      gda_sql_builder_add_param (builder, "attvalue", G_TYPE_STRING, FALSE));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);
	}

	if (! gda_connection_commit_transaction (store_cnc, NULL, NULL)) {
		g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Can't commit transaction to access favorites"));
		goto err;
	}

	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	/*
	g_print ("%s(table=>%s, column=>%s, value=>%s)\n", __FUNCTION__, GDA_META_DB_OBJECT (table)->obj_full_name,
		 column->column_name, value);
	*/
	g_signal_emit (bcnc, browser_connection_signals [TABLE_COLUMN_PREF_CHANGED], 0,
		       table, column, attr_name, value);

	return TRUE;

 err:
	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	gda_connection_rollback_transaction (store_cnc, NULL, NULL);
	return FALSE;
}

/**
 * browser_connection_get_table_column_attribute
 * @bcnc:
 * @dbo:
 * @column: may be %NULL
 * @attr_name: attribute name, not %NULL
 * @error:
 *
 *
 * Returns: the requested attribute (as a new string), or %NULL if not set or if an error occurred
 */
gchar *
browser_connection_get_table_column_attribute  (BrowserConnection *bcnc,
						GdaMetaTable *table,
						GdaMetaTableColumn *column,
						const gchar *attr_name,
						GError **error)
{
	GdaConnection *store_cnc;
	gchar *retval = NULL;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (table, FALSE);
	g_return_val_if_fail (column, FALSE);
	g_return_val_if_fail (attr_name, FALSE);

	if (! bcnc->priv->store_cnc &&
	    ! meta_store_addons_init (bcnc, error))
		return FALSE;

	store_cnc = bcnc->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, TOOLS_ERROR, TOOLS_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}

	/* SELECT */
	GdaStatement *stmt;
	GdaSqlBuilder *builder;
	GdaSet *params;
	GdaSqlBuilderId op_ids[4];
	GdaDataModel *model = NULL;
	const GValue *cvalue;
	GdaMetaDbObject *dbo = (GdaMetaDbObject *) table;

	params = gda_set_new_inline (4, "schema", G_TYPE_STRING, dbo->obj_schema,
				     "name", G_TYPE_STRING, dbo->obj_name,
				     "column", G_TYPE_STRING, column->column_name,
				     "attname", G_TYPE_STRING, attr_name);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target_id (builder,
					      gda_sql_builder_add_id (builder, DBTABLE_PREFERENCES_TABLE_NAME),
					      NULL);
	gda_sql_builder_select_add_field (builder, "att_value", NULL, NULL);
	op_ids[0] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_schema"),
					      gda_sql_builder_add_param (builder, "schema", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[1] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_name"),
					      gda_sql_builder_add_param (builder, "name", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[2] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "table_column"),
					      gda_sql_builder_add_param (builder, "column", G_TYPE_STRING,
									 FALSE), 0);
	op_ids[3] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
					      gda_sql_builder_add_id (builder, "att_name"),
					      gda_sql_builder_add_param (builder, "attname", G_TYPE_STRING,
									 FALSE), 0);
	gda_sql_builder_set_where (builder,
				   gda_sql_builder_add_cond_v (builder, GDA_SQL_OPERATOR_TYPE_AND,
							       op_ids, 4));
	stmt = gda_sql_builder_get_statement (builder, error);
	g_object_unref (G_OBJECT (builder));
	if (!stmt)
		goto out;

	model = gda_connection_statement_execute_select (store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model)
		goto out;

	/*gda_data_model_dump (model, NULL);*/
	if (gda_data_model_get_n_rows (model) == 0)
		goto out;

	cvalue = gda_data_model_get_value_at (model, 0, 0, error);
	if (cvalue)
		retval = g_value_dup_string (cvalue);

 out:
	if (model)
		g_object_unref (model);
	g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));

	return retval;
}

/**
 * browser_connection_define_ui_plugins_for_batch
 * @bcnc: a #BrowserConnection object
 * @batch: a #GdaBatch
 * @params: a #GdaSet (usually created with gda_batch_get_parameters())
 *
 * Calls browser_connection_define_ui_plugins_for_stmt() for each statement in @batch
 */
void
browser_connection_define_ui_plugins_for_batch (BrowserConnection *bcnc, GdaBatch *batch, GdaSet *params)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	g_return_if_fail (GDA_IS_BATCH (batch));
	if (!params)
		return;
	g_return_if_fail (GDA_IS_SET (params));

	const GSList *list;
	for (list = gda_batch_get_statements (batch); list; list = list->next)
		browser_connection_define_ui_plugins_for_stmt (bcnc, GDA_STATEMENT (list->data), params);
}

/* remark: the current ABI leaves no room to add a
 * validity check to the GdaSqlExpr structure, and the following test
 * should be done in gda_sql_expr_check_validity() once the GdaSqlExpr
 * has the capacity to hold the information (ie. when ABI is broken)
 *
 * The code here is a modification from the gda_sql_select_field_check_validity()
 * adapted for the GdaSqlExpr.
 */
static gboolean
_gda_sql_expr_check_validity (GdaSqlExpr *expr, GdaMetaStruct *mstruct,
			      GdaMetaDbObject **out_validity_meta_object,
			      GdaMetaTableColumn **out_validity_meta_table_column, GError **error)
{
	GdaMetaDbObject *dbo = NULL;
	const gchar *field_name;

	*out_validity_meta_object = NULL;
	*out_validity_meta_table_column = NULL;

	if (! expr->value || (G_VALUE_TYPE (expr->value) != G_TYPE_STRING))
		return TRUE;
	field_name = g_value_get_string (expr->value);

	
	GdaSqlAnyPart *any;
	GdaMetaTableColumn *tcol = NULL;
	GValue value;

	memset (&value, 0, sizeof (GValue));
	for (any = GDA_SQL_ANY_PART(expr)->parent;
	     any && (any->type != GDA_SQL_ANY_STMT_SELECT) && (any->type != GDA_SQL_ANY_STMT_DELETE) &&
		     (any->type != GDA_SQL_ANY_STMT_UPDATE);
	     any = any->parent);
	if (!any) {
		/* not in a structure which can be analysed */
		return TRUE;
	}

	switch (any->type) {
	case GDA_SQL_ANY_STMT_SELECT: {
		/* go through all the SELECT's targets to see if
		 * there is a table with the corresponding field */
		GSList *targets;
		if (((GdaSqlStatementSelect *)any)->from) {
			for (targets = ((GdaSqlStatementSelect *)any)->from->targets;
			     targets;
			     targets = targets->next) {
				GdaSqlSelectTarget *target = (GdaSqlSelectTarget *) targets->data;
				if (!target->validity_meta_object /*&&
				     * commented out in the current context because
				     * browser_connection_check_sql_statement_validify() has already been 
				     * called, will need to be re-added when movind to the
				     * gda-statement-struct.c file.
				     *
				     * !gda_sql_select_target_check_validity (target, data, error)*/)
					return FALSE;
				
				g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
				tcol = gda_meta_struct_get_table_column (mstruct,
									 GDA_META_TABLE (target->validity_meta_object),
									 &value);
				g_value_unset (&value);
				if (tcol) {
					/* found a candidate */
					if (dbo) {
						g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
							     _("Could not identify table for field '%s'"), field_name);
						return FALSE;
					}
					dbo = target->validity_meta_object;
				}
			}
		}
		break;
	}
	case GDA_SQL_ANY_STMT_UPDATE: {
		GdaSqlTable *table;
		table = ((GdaSqlStatementUpdate *)any)->table;
		if (!table || !table->validity_meta_object /* ||
		    * commented out in the current context because
		    * browser_connection_check_sql_statement_validify() has already been 
		    * called, will need to be re-added when movind to the
		    * gda-statement-struct.c file.
		    *
		    * !gda_sql_select_target_check_validity (target, data, error)*/)
			return FALSE;
		dbo = table->validity_meta_object;
		g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
		tcol = gda_meta_struct_get_table_column (mstruct,
							 GDA_META_TABLE (table->validity_meta_object),
							 &value);
		g_value_unset (&value);
		break;
	}
	case GDA_SQL_ANY_STMT_DELETE: {
		GdaSqlTable *table;
		table = ((GdaSqlStatementDelete *)any)->table;
		if (!table || !table->validity_meta_object /* ||
		    * commented out in the current context because
		    * browser_connection_check_sql_statement_validify() has already been 
		    * called, will need to be re-added when movind to the
		    * gda-statement-struct.c file.
		    *
		    * !gda_sql_select_target_check_validity (target, data, error)*/)
			return FALSE;
		dbo = table->validity_meta_object;
		g_value_set_string (g_value_init (&value, G_TYPE_STRING), field_name);
		tcol = gda_meta_struct_get_table_column (mstruct,
							 GDA_META_TABLE (table->validity_meta_object),
							 &value);
		g_value_unset (&value);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	if (!dbo) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			     _("Could not identify table for field '%s'"), field_name);
		return FALSE;
	}
	*out_validity_meta_object = dbo;
	*out_validity_meta_table_column = tcol;
	return TRUE;
}

typedef struct {
	BrowserConnection *bcnc;
	GdaSet *params;
} ParamsData;

/*
 *
 * In this function we try to find for which table's column a parameter is and use
 * preferences to set the GdaHolder's plugin attribute
 */
static gboolean
foreach_ui_plugins_for_params (GdaSqlAnyPart *part, ParamsData *data, G_GNUC_UNUSED GError **error)
{
	if (part->type != GDA_SQL_ANY_EXPR)
		return TRUE;
	GdaSqlExpr *expr = (GdaSqlExpr*) part;
	if (!expr->param_spec)
		return TRUE;

	GdaHolder *holder;
	holder = gda_set_get_holder (data->params, expr->param_spec->name);
	if (! holder)
		return TRUE;

	GdaSqlAnyPart *uppart;
	gchar *plugin = NULL;
	uppart = part->parent;
	if (!uppart)
		return TRUE;
	else if (uppart->type == GDA_SQL_ANY_SQL_OPERATION) {
		GdaSqlOperation *op = (GdaSqlOperation*) uppart;
		/* look into condition */
		GSList *list;
		for (list = op->operands; list; list = list->next) {
			GdaSqlExpr *oexpr = (GdaSqlExpr*) list->data;
			if (oexpr == expr)
				continue;
			
			GdaMetaDbObject    *validity_meta_object;
			GdaMetaTableColumn *validity_meta_table_column;
			if (_gda_sql_expr_check_validity (oexpr,
							  browser_connection_get_meta_struct (data->bcnc),
							  &validity_meta_object,
							  &validity_meta_table_column, NULL)) {
				plugin = browser_connection_get_table_column_attribute (data->bcnc,
											GDA_META_TABLE (validity_meta_object),
											validity_meta_table_column,
											BROWSER_CONNECTION_COLUMN_PLUGIN, NULL);
				break;
			}
		}
	}
	else if (uppart->type == GDA_SQL_ANY_STMT_UPDATE) {
		GdaSqlStatementUpdate *upd = (GdaSqlStatementUpdate*) uppart;
		GdaSqlField *field;
		field = g_slist_nth_data (upd->fields_list, g_slist_index (upd->expr_list, expr));
		if (field)
			plugin = browser_connection_get_table_column_attribute (data->bcnc,
										GDA_META_TABLE (upd->table->validity_meta_object),
										field->validity_meta_table_column,
										BROWSER_CONNECTION_COLUMN_PLUGIN, NULL);
	}
	else if (uppart->type == GDA_SQL_ANY_STMT_INSERT) {
		GdaSqlStatementInsert *ins = (GdaSqlStatementInsert*) uppart;
		GdaSqlField *field;
		gint expr_index = -1;
		GSList *slist;
		GdaMetaTableColumn *column = NULL;
		for (slist = ins->values_list; slist; slist = slist->next) {
			expr_index = g_slist_index ((GSList*) slist->data, expr);
			if (expr_index >= 0)
				break;
		}
		if (expr_index >= 0) {
			field = g_slist_nth_data (ins->fields_list, expr_index);
			if (field)
				column = field->validity_meta_table_column;
			else {
				/* no field specified => take the table's fields */
				GdaMetaTable *mtable = GDA_META_TABLE (ins->table->validity_meta_object);
				column = g_slist_nth_data (mtable->columns, expr_index);
			}
		}
		if (column)
			plugin = browser_connection_get_table_column_attribute (data->bcnc,
										GDA_META_TABLE (ins->table->validity_meta_object),
										column,
										BROWSER_CONNECTION_COLUMN_PLUGIN, NULL);
	}

	if (plugin) {
		/*g_print ("Using plugin [%s]\n", plugin);*/
		GValue *value;
		g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), plugin);
		gda_holder_set_attribute_static (holder, GDAUI_ATTRIBUTE_PLUGIN, value);
		gda_value_free (value);
	}

	return TRUE;
}

/**
 * browser_connection_define_ui_plugins_for_stmt
 * @bcnc: a #BrowserConnection object
 * @stmt: a #GdaStatement
 * @params: a #GdaSet (usually created with gda_statement_get_parameters())
 *
 * Analyses @stmt and assign plugins to each #GdaHolder in @params according to the preferences stored
 * for each table's field, defined at some point using browser_connection_set_table_column_attribute().
 */
void
browser_connection_define_ui_plugins_for_stmt (BrowserConnection *bcnc, GdaStatement *stmt, GdaSet *params)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	g_return_if_fail (GDA_IS_STATEMENT (stmt));
	if (!params)
		return;
	g_return_if_fail (GDA_IS_SET (params));
	
	GdaSqlStatement *sqlst;
	GdaSqlAnyPart *rootpart;
	g_object_get ((GObject*) stmt, "structure", &sqlst, NULL);
	g_return_if_fail (sqlst);
	switch (sqlst->stmt_type) {
	case GDA_SQL_STATEMENT_INSERT:
	case GDA_SQL_STATEMENT_UPDATE:
	case GDA_SQL_STATEMENT_DELETE:
	case GDA_SQL_STATEMENT_SELECT:
	case GDA_SQL_STATEMENT_COMPOUND:
		rootpart = (GdaSqlAnyPart*) sqlst->contents;
		break;
	default:
		rootpart = NULL;
		break;
	}
	GError *lerror = NULL;
	if (!rootpart || !browser_connection_check_sql_statement_validify (bcnc, sqlst, &lerror)) {
		/*g_print ("ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");*/
		g_clear_error (&lerror);
		gda_sql_statement_free (sqlst);
		return;
	}
	
	ParamsData data;
	data.params = params;
	data.bcnc = bcnc;
	gda_sql_any_part_foreach (rootpart, (GdaSqlForeachFunc) foreach_ui_plugins_for_params,
				  &data, NULL);
	
	gda_sql_statement_free (sqlst);

	/* REM: we also need to handle FK tables to propose a drop down list of possible values */
}

/**
 * browser_connection_keep_variables
 * @bcnc: a #BrowserConnection object
 * @set: a #GdaSet containing variables for which a copy has to be done
 *
 * Makes a copy of the variables in @set and keep them in @bcnc. Retreive them
 * using browser_connection_load_variables()
 */
void
browser_connection_keep_variables (BrowserConnection *bcnc, GdaSet *set)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	if (!set)
		return;
	g_return_if_fail (GDA_IS_SET (set));

	if (! bcnc->priv->variables) {
		bcnc->priv->variables = gda_set_copy (set);
		return;
	}

	GSList *list;
	for (list = set->holders; list; list = list->next) {
		GdaHolder *nh, *eh;
		nh = GDA_HOLDER (list->data);
		eh = gda_set_get_holder (bcnc->priv->variables, gda_holder_get_id (nh));
		if (eh) {
			if (gda_holder_get_g_type (nh) == gda_holder_get_g_type (eh)) {
				const GValue *cvalue;
				cvalue = gda_holder_get_value (nh);
				gda_holder_set_value (eh, cvalue, NULL);
			}
			else {
				gda_set_remove_holder (bcnc->priv->variables, eh);
				eh = gda_holder_copy (nh);
				gda_set_add_holder (bcnc->priv->variables, eh);
				g_object_unref (eh);
			}
		}
		else {
			eh = gda_holder_copy (nh);
			gda_set_add_holder (bcnc->priv->variables, eh);
			g_object_unref (eh);
		}
	}
}

/**
 * browser_connection_load_variables
 * @bcnc: a #BrowserConnection object
 * @set: a #GdaSet which will in the end contain (if any) variables stored in @bcnc
 *
 * For each #GdaHolder in @set, set the value if one is available in @bcnc.
 */
void
browser_connection_load_variables (BrowserConnection *bcnc, GdaSet *set)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));
	if (!set)
		return;
	g_return_if_fail (GDA_IS_SET (set));

	if (! bcnc->priv->variables)
		return;

	GSList *list;
	for (list = set->holders; list; list = list->next) {
		GdaHolder *nh, *eh;
		nh = GDA_HOLDER (list->data);
		eh = gda_set_get_holder (bcnc->priv->variables, gda_holder_get_id (nh));
		if (eh) {
			if (gda_holder_get_g_type (nh) == gda_holder_get_g_type (eh)) {
				const GValue *cvalue;
				cvalue = gda_holder_get_value (eh);
				gda_holder_set_value (nh, cvalue, NULL);
			}
			else if (g_value_type_transformable (gda_holder_get_g_type (eh),
							     gda_holder_get_g_type (nh))) {
				const GValue *evalue;
				GValue *nvalue;
				evalue = gda_holder_get_value (eh);
				nvalue = gda_value_new (gda_holder_get_g_type (nh));
				if (g_value_transform (evalue, nvalue))
					gda_holder_take_value (nh, nvalue, NULL);
				else
					gda_value_free (nvalue);
			}
		}
	}	
}

/**
 * browser_connection_is_ldap:
 * @bcnc: a #BrowserConnection
 *
 * Returns: %TRUE if @bcnc proxies an LDAP connection
 */
gboolean
browser_connection_is_ldap (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);

#ifdef HAVE_LDAP
	return GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc) ? TRUE : FALSE;
#endif
	return FALSE;
}

#ifdef HAVE_LDAP

typedef struct {
	GdaConnection *cnc;
	gchar *base_dn;
	gchar *attributes;
	gchar *filter;
	GdaLdapSearchScope scope;
} LdapSearchData;
static void
ldap_search_data_free (LdapSearchData *data)
{
	g_free (data->base_dn);
	g_free (data->filter);
	g_free (data->attributes);
	g_free (data);
}

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_ldap_search (LdapSearchData *data, GError **error)
{
	GdaDataModel *model;
	model = gda_data_model_ldap_new (GDA_CONNECTION (data->cnc), data->base_dn,
					 data->filter, data->attributes, data->scope);
	if (!model) {
		g_set_error (error, TOOLS_ERROR, TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not execute LDAP search"));
		return (gpointer) 0x01;
	}
	else {
		GdaDataModel *wrapped;
		wrapped = gda_data_access_wrapper_new (model);
		g_object_unref (model);
		/* force loading all the LDAP entries in memory to avoid
		 * having the GTK thread lock on LDAP searches */
		gda_data_model_get_n_rows (wrapped);
		return wrapped;
	}
}

/**
 * browser_connection_ldap_search:
 *
 * Executes an LDAP search. Wrapper around gda_data_model_ldap_new()
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_ldap_search (BrowserConnection *bcnc,
				const gchar *base_dn, const gchar *filter,
				const gchar *attributes, GdaLdapSearchScope scope,
				BrowserConnectionJobCallback callback,
				gpointer cb_data, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc), 0);

	LdapSearchData *data;
	guint job_id;
	data = g_new0 (LdapSearchData, 1);
	data->cnc = bcnc->priv->cnc;
	data->base_dn = g_strdup (base_dn);
	data->filter = g_strdup (filter);
	data->attributes = g_strdup (attributes);
	data->scope = scope;

	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_ldap_search,
					     data, (GDestroyNotify) ldap_search_data_free, error);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_CALLBACK,
				  _("Executing LDAP search"), callback, cb_data);
	return job_id;
}


typedef struct {
	GdaConnection *cnc;
	gchar *dn;
	gchar **attributes;
} LdapData;
static void
ldap_data_free (LdapData *data)
{
	g_free (data->dn);
	if (data->attributes)
		g_strfreev (data->attributes);
	g_free (data);
}

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_ldap_describe_entry (LdapData *data, GError **error)
{
	GdaLdapEntry *lentry;
	lentry = gda_ldap_describe_entry (GDA_LDAP_CONNECTION (data->cnc), data->dn, error);
	return lentry ? lentry : (gpointer) 0x01;
}

/**
 * browser_connection_ldap_describe_entry:
 * @bcnc: a #BrowserConnection
 * @dn: the DN of the entry to describe
 * @callback: the callback to execute with the results
 * @cb_data: a pointer to pass to @callback
 * @error: a place to store errors, or %NULL
 *
 * Wrapper around gda_ldap_describe_entry().
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_ldap_describe_entry (BrowserConnection *bcnc, const gchar *dn,
					BrowserConnectionJobCallback callback,
					gpointer cb_data, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc), 0);

	LdapData *data;
	guint job_id;

	data = g_new0 (LdapData, 1);
	data->cnc = bcnc->priv->cnc;
	data->dn = g_strdup (dn);

	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_ldap_describe_entry,
					     data, (GDestroyNotify) ldap_data_free, error);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_CALLBACK,
				  _("Fetching LDAP entry's attributes"), callback, cb_data);

	return job_id;
}

/* executed in sub @bcnc->priv->wrapper's thread */
static gpointer
wrapper_ldap_get_entry_children (LdapData *data, GError **error)
{
	GdaLdapEntry **array;
	array = gda_ldap_get_entry_children (GDA_LDAP_CONNECTION (data->cnc), data->dn, data->attributes, error);
	return array ? array : (gpointer) 0x01;
}

/**
 * browser_connection_ldap_get_entry_children:
 * @bcnc: a #BrowserConnection
 * @dn: the DN of the entry to get children from
 * @callback: the callback to execute with the results
 * @cb_data: a pointer to pass to @callback
 * @error: a place to store errors, or %NULL
 *
 * Wrapper around gda_ldap_get_entry_children().
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_ldap_get_entry_children (BrowserConnection *bcnc, const gchar *dn,
					    gchar **attributes,
					    BrowserConnectionJobCallback callback,
					    gpointer cb_data, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc), 0);

	LdapData *data;
	guint job_id;

	data = g_new0 (LdapData, 1);
	data->cnc = bcnc->priv->cnc;
	data->dn = g_strdup (dn);
	if (attributes) {
		gint i;
		GArray *array = NULL;
		for (i = 0; attributes [i]; i++) {
			gchar *tmp;
			if (! array)
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
			tmp = g_strdup (attributes [i]);
			g_array_append_val (array, tmp);
		}
		if (array)
			data->attributes = (gchar**) g_array_free (array, FALSE);
	}

	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_ldap_get_entry_children,
					     data, (GDestroyNotify) ldap_data_free, error);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_CALLBACK,
				  _("Fetching LDAP entry's children"), callback, cb_data);

	return job_id;
}

/**
 * browser_connection_ldap_icon_for_class:
 * @objectclass: objectClass attribute
 *
 * Returns: the correct icon, or %NULL if it could not be determined
 */
GdkPixbuf *
browser_connection_ldap_icon_for_class (GdaLdapAttribute *objectclass)
{
	gint type = 0;
	if (objectclass) {
		guint i;
		for (i = 0; i < objectclass->nb_values; i++) {
			const gchar *class = g_value_get_string (objectclass->values[i]);
			if (!class)
				continue;
			else if (!strcmp (class, "organization"))
				type = MAX(type, 1);
			else if (!strcmp (class, "groupOfNames") ||
				 !strcmp (class, "posixGroup"))
				type = MAX(type, 2);
			else if (!strcmp (class, "account") ||
				 !strcmp (class, "mailUser") ||
				 !strcmp (class, "organizationalPerson") ||
				 !strcmp (class, "person") ||
				 !strcmp (class, "pilotPerson") ||
				 !strcmp (class, "newPilotPerson") ||
				 !strcmp (class, "pkiUser") ||
				 !strcmp (class, "posixUser") ||
				 !strcmp (class, "posixAccount") ||
				 !strcmp (class, "residentalPerson") ||
				 !strcmp (class, "shadowAccount") ||
				 !strcmp (class, "strongAuthenticationUser") ||
				 !strcmp (class, "inetOrgPerson"))
				type = MAX(type, 3);
		}
	}

	switch (type) {
	case 0:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_ENTRY);
	case 1:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_ORGANIZATION);
	case 2:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_GROUP);
	case 3:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_PERSON);
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

typedef struct {
	BrowserConnectionJobCallback callback;
	gpointer cb_data;
} IconTypeData;

static void
fetch_classes_cb (G_GNUC_UNUSED BrowserConnection *bcnc,
		  gpointer out_result, IconTypeData *data, G_GNUC_UNUSED GError *error)
{
	GdkPixbuf *pixbuf = NULL;
	if (out_result) {
		GdaLdapEntry *lentry = (GdaLdapEntry*) out_result;
		GdaLdapAttribute *attr;
		attr = g_hash_table_lookup (lentry->attributes_hash, "objectClass");
		pixbuf = browser_connection_ldap_icon_for_class (attr);
		gda_ldap_entry_free (lentry);
	}
	if (data->callback)
		data->callback (bcnc, pixbuf, data->cb_data, error);

	g_free (data);
}

/**
 * browser_connection_ldap_icon_for_dn:
 * @bcnc: a #BrowserConnection
 * @dn: the DN of the entry
 * @callback: the callback to execute with the results
 * @cb_data: a pointer to pass to @callback
 * @error: a place to store errors, or %NULL
 *
 * Determines the correct icon type for @dn based on which class it belongs to
 *
 * Returns: a job ID, or %0 if an error occurred
 */
guint
browser_connection_ldap_icon_for_dn (BrowserConnection *bcnc, const gchar *dn,
				     BrowserConnectionJobCallback callback,
				     gpointer cb_data, GError **error)
{
	IconTypeData *data;
	guint job_id;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), 0);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc), 0);
	g_return_val_if_fail (dn && *dn, 0);

	data = g_new (IconTypeData, 1);
	data->callback = callback;
	data->cb_data = cb_data;
	job_id = browser_connection_ldap_describe_entry (bcnc, dn,
							 BROWSER_CONNECTION_JOB_CALLBACK (fetch_classes_cb),
							 data, error);
	return job_id;
}

/**
 * browser_connection_ldap_get_base_dn:
 * @bcnc: a #BrowserConnection
 *
 * wrapper for gda_ldap_connection_get_base_dn()
 *
 * Returns: the base DN or %NULL
 */
const gchar *
browser_connection_ldap_get_base_dn (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (bcnc->priv->cnc), NULL);

	return gda_ldap_connection_get_base_dn (GDA_LDAP_CONNECTION (bcnc->priv->cnc));
}

/**
 * browser_connection_describe_table:
 * @bcnc: a #BrowserConnection
 * @table_name: a table name, not %NULL
 * @out_base_dn: (allow-none) (transfer none): a place to store the LDAP search base DN, or %NULL
 * @out_filter: (allow-none) (transfer none): a place to store the LDAP search filter, or %NULL
 * @out_attributes: (allow-none) (transfer none): a place to store the LDAP search attributes, or %NULL
 * @out_scope: (allow-none) (transfer none): a place to store the LDAP search scope, or %NULL
 * @error: a place to store errors, or %NULL
 */
gboolean
browser_connection_describe_table  (BrowserConnection *bcnc, const gchar *table_name,
				    const gchar **out_base_dn, const gchar **out_filter,
				    const gchar **out_attributes,
				    GdaLdapSearchScope *out_scope, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (browser_connection_is_ldap (bcnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	return gda_ldap_connection_describe_table (GDA_LDAP_CONNECTION (bcnc->priv->cnc),
						   table_name, out_base_dn, out_filter,
						   out_attributes, out_scope, error);
}

/**
 * browser_connection_get_class_info:
 * @bcnc: a #BrowserConnection
 * @classname: a class name
 *
 * proxy for gda_ldap_get_class_info.
 */
GdaLdapClass *
browser_connection_get_class_info (BrowserConnection *bcnc, const gchar *classname)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (browser_connection_is_ldap (bcnc), NULL);

	return gda_ldap_get_class_info (GDA_LDAP_CONNECTION (bcnc->priv->cnc), classname);
}

/**
 * browser_connection_get_top_classes:
 * @bcnc: a #BrowserConnection
 *
 * proxy for gda_ldap_get_top_classes.
 */
const GSList *
browser_connection_get_top_classes (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (browser_connection_is_ldap (bcnc), NULL);

	return gda_ldap_get_top_classes (GDA_LDAP_CONNECTION (bcnc->priv->cnc));
}

/**
 * browser_connection_declare_table:
 * @bcnc: a #BrowserConnection
 *
 * Wrapper around gda_ldap_connection_declare_table()
 */
gboolean
browser_connection_declare_table (BrowserConnection *bcnc,
				  const gchar *table_name,
				  const gchar *base_dn,
				  const gchar *filter,
				  const gchar *attributes,
				  GdaLdapSearchScope scope,
				  GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (browser_connection_is_ldap (bcnc), FALSE);

	return gda_ldap_connection_declare_table (GDA_LDAP_CONNECTION (bcnc->priv->cnc),
						  table_name, base_dn, filter,
						  attributes, scope, error);
}

/**
 * browser_connection_undeclare_table:
 * @bcnc: a #BrowserConnection
 *
 * Wrapper around gda_ldap_connection_undeclare_table()
 */
gboolean
browser_connection_undeclare_table (BrowserConnection *bcnc,
				    const gchar *table_name,
				    GError **error)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (browser_connection_is_ldap (bcnc), FALSE);

	return gda_ldap_connection_undeclare_table (GDA_LDAP_CONNECTION (bcnc->priv->cnc),
						    table_name, error);
}

#endif /* HAVE_LDAP */
