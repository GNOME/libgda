/*
 * Copyright (C) 2009 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "browser-connection.h"
#include <libgda/thread-wrapper/gda-thread-wrapper.h>
#include "support.h"
#include "marshal.h"
#include <sql-parser/gda-sql-parser.h>

/* code inclusion */
#include "../dict-file-name.c"

struct _BrowserConnectionPrivate {
	GdaThreadWrapper *wrapper;
	GSList           *wrapper_jobs;
	guint             wrapper_results_timer;

	gchar         *name;
	GdaConnection *cnc;
	gchar         *dict_file_name;
        GdaSqlParser  *parser;
        GSList        *variables; /* list of BrowserVariable pointer, owned here */

	GdaDsnInfo     dsn_info;
	GMutex        *p_mstruct_mutex;
	GdaMetaStruct *p_mstruct; /* private GdaMetaStruct: while it is being created */
	GdaMetaStruct *mstruct; /* public GdaMetaStruct: once it has been created and is no more modified */

	gboolean       meta_store_addons_init_done;
};


/* wrapper jobs handling */
static gboolean check_for_wrapper_result (BrowserConnection *bcnc);

typedef enum {
	JOB_TYPE_META_STORE_UPDATE,
	JOB_TYPE_META_STRUCT_SYNC,
	JOB_TYPE_STATEMENT_EXECUTE
} JobType;

typedef struct {
	guint    job_id;
	JobType  job_type;
	gchar   *reason;
} WrapperJob;

static void
wrapper_job_free (WrapperJob *wj)
{
	g_free (wj->reason);
	g_free (wj);
}

static void
push_wrapper_job (BrowserConnection *bcnc, guint job_id, JobType job_type, const gchar *reason)
{
	/* setup timer */
	if (bcnc->priv->wrapper_results_timer == 0)
		bcnc->priv->wrapper_results_timer = g_timeout_add (200,
								   (GSourceFunc) check_for_wrapper_result,
								   bcnc);

	/* add WrapperJob structure */
	WrapperJob *wj;
	wj = g_new0 (WrapperJob, 1);
	wj->job_id = job_id;
	wj->job_type = job_type;
	if (reason)
		wj->reason = g_strdup (reason);
	bcnc->priv->wrapper_jobs = g_slist_append (bcnc->priv->wrapper_jobs, wj);
}

static void
pop_wrapper_job (BrowserConnection *bcnc, WrapperJob *wj)
{
	bcnc->priv->wrapper_jobs = g_slist_remove (bcnc->priv->wrapper_jobs, wj);
	wrapper_job_free (wj);
}


/* 
 * Main static functions 
 */
static void browser_connection_class_init (BrowserConnectionClass *klass);
static void browser_connection_init (BrowserConnection *bcnc);
static void browser_connection_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum {
	BUSY,
	META_CHANGED,
	FAV_CHANGED,
	LAST_SIGNAL
};

gint browser_connection_signals [LAST_SIGNAL] = { 0, 0, 0 };

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
			(GInstanceInitFunc) browser_connection_init
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
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (BrowserConnectionClass, favorites_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);

	klass->busy = NULL;
	klass->meta_changed = NULL;
	klass->favorites_changed = NULL;

	object_class->dispose = browser_connection_dispose;
}

static void
browser_connection_init (BrowserConnection *bcnc)
{
	static guint index = 1;
	bcnc->priv = g_new0 (BrowserConnectionPrivate, 1);
	bcnc->priv->wrapper = gda_thread_wrapper_new ();
	bcnc->priv->wrapper_jobs = NULL;
	bcnc->priv->wrapper_results_timer = 0;

	bcnc->priv->name = g_strdup_printf (_("c%u"), index++);
	bcnc->priv->cnc = NULL;
	bcnc->priv->parser = NULL;
	bcnc->priv->variables = NULL;
	memset (&(bcnc->priv->dsn_info), 0, sizeof (GdaDsnInfo));
	bcnc->priv->p_mstruct_mutex = g_mutex_new ();
	bcnc->priv->p_mstruct = NULL;
	bcnc->priv->mstruct = NULL;

	bcnc->priv->meta_store_addons_init_done = FALSE;
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
browser_connection_dispose (GObject *object)
{
	BrowserConnection *bcnc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_CONNECTION (object));

	bcnc = BROWSER_CONNECTION (object);
	if (bcnc->priv) {
		clear_dsn_info (bcnc);

		g_free (bcnc->priv->dict_file_name);

		if (bcnc->priv->wrapper_jobs) {
			g_slist_foreach (bcnc->priv->wrapper_jobs, (GFunc) wrapper_job_free, NULL);
			g_slist_free (bcnc->priv->wrapper_jobs);
		}

		if (bcnc->priv->wrapper_results_timer > 0)
			g_source_remove (bcnc->priv->wrapper_results_timer);

		g_object_unref (bcnc->priv->wrapper);
		g_free (bcnc->priv->name);
		if (bcnc->priv->mstruct)
			g_object_unref (bcnc->priv->mstruct);
		if (bcnc->priv->p_mstruct)
			g_object_unref (bcnc->priv->p_mstruct);
		if (bcnc->priv->p_mstruct_mutex)
			g_mutex_free (bcnc->priv->p_mstruct_mutex);
		if (bcnc->priv->cnc)
			g_object_unref (bcnc->priv->cnc);
		if (bcnc->priv->parser)
			g_object_unref (bcnc->priv->parser);
		if (bcnc->priv->variables) {
			g_slist_foreach (bcnc->priv->variables, (GFunc) g_object_unref, NULL);
			g_slist_free (bcnc->priv->variables);
		}

		g_free (bcnc->priv);
		bcnc->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
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
	gboolean retval;

	g_mutex_lock (bcnc->priv->p_mstruct_mutex);
	g_assert (bcnc->priv->p_mstruct);
	g_object_ref (G_OBJECT (bcnc->priv->p_mstruct));
	retval = gda_meta_struct_complement_all (bcnc->priv->p_mstruct, error);
	g_object_unref (G_OBJECT (bcnc->priv->p_mstruct));
	g_mutex_unlock (bcnc->priv->p_mstruct_mutex);

	return GINT_TO_POINTER (retval ? 2 : 1);
}

static gboolean
check_for_wrapper_result (BrowserConnection *bcnc)
{
	GError *lerror = NULL;
	gpointer exec_res = NULL;
	WrapperJob *wj;

	if (!bcnc->priv->wrapper_jobs)
		goto out;

	wj = (WrapperJob*) bcnc->priv->wrapper_jobs->data;
	exec_res = gda_thread_wrapper_fetch_result (bcnc->priv->wrapper,
						    FALSE, 
						    wj->job_id, &lerror);
	if (exec_res) {
		switch (wj->job_type) {
		case JOB_TYPE_META_STORE_UPDATE: {
			guint job_id;

			g_mutex_lock (bcnc->priv->p_mstruct_mutex);
			if (bcnc->priv->p_mstruct)
				g_object_unref (G_OBJECT (bcnc->priv->p_mstruct));
			bcnc->priv->p_mstruct = gda_meta_struct_new (gda_connection_get_meta_store (bcnc->priv->cnc),
								   GDA_META_STRUCT_FEATURE_ALL);
			g_mutex_unlock (bcnc->priv->p_mstruct_mutex);
			job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
							     (GdaThreadWrapperFunc) wrapper_meta_struct_sync,
							     g_object_ref (bcnc), g_object_unref, &lerror);
			if (job_id > 0)
				push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STRUCT_SYNC,
						  _("Analysing database schema"));
			else if (lerror) {
				browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
						    lerror->message ? lerror->message : _("No detail"));
				g_error_free (lerror);
			}
			break;
		}
		case JOB_TYPE_META_STRUCT_SYNC: {
#ifdef GDA_DEBUG_NO
			GSList *all, *list;
			g_mutex_lock (bcnc->priv->p_mstruct_mutex);
			all = gda_meta_struct_get_all_db_objects (bcnc->priv->p_mstruct);
			g_mutex_unlock (bcnc->priv->p_mstruct_mutex);
			for (list = all; list; list = list->next) {
				GdaMetaDbObject *dbo = (GdaMetaDbObject *) list->data;
				g_print ("DBO, Type %d: %s\n", dbo->obj_type,
					 dbo->obj_short_name);
			}
			g_slist_free (all);
#endif
			g_mutex_lock (bcnc->priv->p_mstruct_mutex);
			if (bcnc->priv->mstruct)
				g_object_unref (bcnc->priv->mstruct);
			bcnc->priv->mstruct = bcnc->priv->p_mstruct;
			bcnc->priv->p_mstruct = NULL;
			g_signal_emit (bcnc, browser_connection_signals [META_CHANGED], 0, bcnc->priv->mstruct);
			g_mutex_unlock (bcnc->priv->p_mstruct_mutex);
			break;
		}
		case JOB_TYPE_STATEMENT_EXECUTE:
			TO_IMPLEMENT;
			break;
		}

		pop_wrapper_job (bcnc, wj);
	}

 out:
	if (!bcnc->priv->wrapper_jobs) {
		bcnc->priv->wrapper_results_timer = 0;
		g_signal_emit (bcnc, browser_connection_signals [BUSY], 0, FALSE, NULL);
		return FALSE;
	}
	else {
		wj = (WrapperJob*) bcnc->priv->wrapper_jobs->data;
		if (exec_res)
			g_signal_emit (bcnc, browser_connection_signals [BUSY], 0, TRUE, wj->reason);
		return TRUE;
	}
}

/**
 * browser_connection_new
 *
 * Creates a new #BrowserConnection object
 *
 * Returns: the new object
 */
BrowserConnection*
browser_connection_new (GdaConnection *cnc)
{
	BrowserConnection *bcnc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	bcnc = BROWSER_CONNECTION (g_object_new (BROWSER_TYPE_CONNECTION, NULL));
	bcnc->priv->cnc = g_object_ref (cnc);

	/* meta store */
	gchar *dict_file_name = NULL;
	gboolean update_store = FALSE;
	GdaMetaStore *store;
	gchar *cnc_string, *cnc_info;
	
	g_object_get (G_OBJECT (cnc),
		      "dsn", &cnc_info,
		      "cnc-string", &cnc_string, NULL);
	dict_file_name = compute_dict_file_name (cnc_info ? gda_config_get_dsn_info (cnc_info) : NULL,
						 cnc_string);
	g_free (cnc_string);
	if (dict_file_name) {
		if (! g_file_test (dict_file_name, G_FILE_TEST_EXISTS))
			update_store = TRUE;
		store = gda_meta_store_new_with_file (dict_file_name);
	}
	else {
		store = gda_meta_store_new (NULL);
		if (store)
			update_store = TRUE;
	}
	bcnc->priv->dict_file_name = dict_file_name;
	g_object_set (G_OBJECT (cnc), "meta-store", store, NULL);
	if (update_store) {
		GError *lerror = NULL;
		guint job_id;
		/*g_print ("UPDATING meta store...\n");*/
		job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
						     (GdaThreadWrapperFunc) wrapper_meta_store_update,
						     g_object_ref (bcnc), g_object_unref, &lerror);
		if (job_id > 0)
			push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STORE_UPDATE,
					  _("Getting database schema information"));
		else if (lerror) {
			browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
					    lerror->message ? lerror->message : _("No detail"));
			g_error_free (lerror);
		}
	}
	else {
		guint job_id;
		GError *lerror = NULL;

		bcnc->priv->p_mstruct = gda_meta_struct_new (store, GDA_META_STRUCT_FEATURE_ALL);
		job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
						     (GdaThreadWrapperFunc) wrapper_meta_struct_sync,
						     g_object_ref (bcnc), g_object_unref, &lerror);
		if (job_id > 0)
			push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STRUCT_SYNC,
					  _("Analysing database schema"));
		else if (lerror) {
			browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
					    lerror->message ? lerror->message : _("No detail"));
			g_error_free (lerror);
		}
		g_object_unref (store);
	}

	return bcnc;
}

/**
 * browser_connection_get_name
 */
const gchar *
browser_connection_get_name (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return bcnc->priv->name;
}

/**
 * browser_connection_get_information
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
 * browser_connection_is_busy
 * @bcnc: a #BrowserConnection
 * @out_reason: a pointer to store a copy of the reason @bcnc is busy (will be set 
 *              to %NULL if @bcnc is not busy)
 */
gboolean
browser_connection_is_busy (BrowserConnection *bcnc, gchar **out_reason)
{
	if (out_reason)
		*out_reason = NULL;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	if (bcnc->priv->wrapper_jobs) {
		WrapperJob *wj = (WrapperJob*) bcnc->priv->wrapper_jobs->data;
		if (out_reason && wj->reason)
			*out_reason = g_strdup (wj->reason);
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * browser_connection_update_meta_data
 */
void
browser_connection_update_meta_data (BrowserConnection *bcnc)
{
	g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));

	if (bcnc->priv->wrapper_jobs) {
		WrapperJob *wj;
		wj = (WrapperJob*) g_slist_last (bcnc->priv->wrapper_jobs)->data;
		if (wj->job_type == JOB_TYPE_META_STORE_UPDATE)
			/* nothing to do */
			return;
	}

	guint job_id;
	GError *lerror = NULL;
	job_id = gda_thread_wrapper_execute (bcnc->priv->wrapper,
					     (GdaThreadWrapperFunc) wrapper_meta_store_update,
					     g_object_ref (bcnc), g_object_unref, &lerror);
	if (job_id > 0)
		push_wrapper_job (bcnc, job_id, JOB_TYPE_META_STORE_UPDATE,
				  _("Getting database schema information"));
	else if (lerror) {
		browser_show_error (NULL, _("Error while fetching meta data from the connection: %s"),
				    lerror->message ? lerror->message : _("No detail"));
		g_error_free (lerror);
	}
}

/**
 * browser_connection_get_meta_struct
 *
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
 *
 *
 * Returns: a #GdaMetaStore, the caller does not have any reference to it.
 */
GdaMetaStore *
browser_connection_get_meta_store (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return gda_connection_get_meta_store (bcnc->priv->cnc);
}

/**
 * browser_connection_get_dictionary_file
 *
 * Returns: the dictionary file name used by @bcnc, or %NULL
 */
const gchar *
browser_connection_get_dictionary_file (BrowserConnection *bcnc)
{
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	return bcnc->priv->dict_file_name;
}

#define FAVORITES_TABLE_NAME "gda_sql_favorites"
#define FAVORITES_TABLE_DESC \
        "<table name=\"" FAVORITES_TABLE_NAME "\"> "                            \
        "   <column name=\"session\" type=\"gint\" pkey=\"TRUE\"/>"             \
        "   <column name=\"type\" pkey=\"TRUE\"/>"                              \
        "   <column name=\"contents\" pkey=\"TRUE\"/>"                          \
        "   <column name=\"descr\" nullok=\"TRUE\"/>"                           \
        "   <column name=\"rank\" type=\"gint\"/>"                             \
        "   <unique>"                             \
        "     <column name=\"type\"/>"                             \
        "     <column name=\"rank\"/>"                             \
        "   </unique>"                             \
        "</table>"
#define FAVORITE_DELETE \
        "DELETE FROM " FAVORITES_TABLE_NAME " WHERE session = ##session::gint AND type= ##type::string AND " \
	"contents = ##contents::string"
#define FAVORITE_SHIFT \
	"UPDATE " FAVORITES_TABLE_NAME " SET rank = rank + 1 WHERE rank >= ##rank::gint"
#define FAVORITE_INSERT \
        "INSERT INTO " FAVORITES_TABLE_NAME " (session, type, contents, rank, descr) VALUES (##session::gint, " \
	"##type::string, ##contents::string, ##rank::gint, ##descr::string)"
#define FAVORITE_SELECT \
        "SELECT contents, descr, rank FROM " FAVORITES_TABLE_NAME " WHERE session = ##session::gint " \
	"AND type = ##type::string ORDER by rank"
#define FAVORITE_UPDATE_RANK \
	"UPDATE " FAVORITES_TABLE_NAME " SET rank = ##newrank::gint WHERE rank = ##rank::gint AND " \
	"session = ##session::gint AND type = ##type::string"

static gboolean
meta_store_addons_init (BrowserConnection *bcnc, GError **error)
{
	GdaMetaStore *mstore;
	GError *lerror = NULL;
	mstore = gda_connection_get_meta_store (bcnc->priv->cnc);
	if (!gda_meta_store_schema_add_custom_object (mstore, FAVORITES_TABLE_DESC, &lerror)) {
                g_set_error (error, 0, 0, "%s",
                             _("Can't initialize dictionary to store favorites"));
		g_warning ("Can't initialize dictionary to store favorites :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }
	
	bcnc->priv->meta_store_addons_init_done = TRUE;
	return TRUE;
}

static const gchar *
favorite_type_to_string (BrowserFavoritesType type)
{
	switch (type) {
	case BROWSER_FAVORITES_TABLES:
		return "TABLE";
	default:
		g_warning ("Unknow type of favorite");
	}

	return "";
}

/**
 * browser_connection_add_favorite
 */
gboolean
browser_connection_add_favorite (BrowserConnection *bcnc, guint session_id, BrowserConnectionFavorite *fav,
				 gint pos, GError **error)
{
	GdaMetaStore *mstore;
	GdaConnection *store_cnc;
	GdaStatement *stmt;
	GdaSqlParser *parser = NULL;
	GdaSet *params = NULL;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (fav && fav->contents, FALSE);

	if (! bcnc->priv->meta_store_addons_init_done &&
	    ! meta_store_addons_init (bcnc, error))
		return FALSE;

	mstore = gda_connection_get_meta_store (bcnc->priv->cnc);
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (store_cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
                return FALSE;
	}

	/* create parser */
	parser = gda_connection_create_parser (store_cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	/* DELETE any favorite existing */
	if (pos < 0)
		pos = 0;
	params = gda_set_new_inline (6,
				     "session", G_TYPE_INT, session_id,
				     "type", G_TYPE_STRING, favorite_type_to_string (fav->type),
				     "contents", G_TYPE_STRING, fav->contents,
				     "rank", G_TYPE_INT, pos,
				     "newrank", G_TYPE_INT, 0,
				     "descr", G_TYPE_STRING, fav->descr);
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_DELETE, NULL, error);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_DELETE);
		goto err;
	}
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto err;
	}
	g_object_unref (stmt);
	stmt = NULL;

	/* renumber favorites */
	GdaDataModel *model;
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_SELECT, NULL, NULL);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_SELECT);
		goto err;
	}
	model = gda_connection_statement_execute_select (store_cnc, stmt, params, error);
	g_object_unref (stmt);
	stmt = NULL;
	if (!model)
		goto err;
	
	gint i, nrows;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *rank;

		rank = gda_data_model_get_value_at (model, 2, i, error);
		if (!rank) {
			g_set_error (error, 0, 0, "%s",
				     _("Can't get current favorites"));
			g_object_unref (model);
			goto err;
		}
		if (g_value_get_int (rank) != i) {
			if (!stmt) {
				stmt = gda_sql_parser_parse_string (parser, FAVORITE_UPDATE_RANK, NULL, NULL);
				if (!stmt) {
					g_warning ("Could not parse internal statement: %s", FAVORITE_UPDATE_RANK);
					g_object_unref (model);
					goto err;
				}
			}
			gda_set_set_holder_value (params, NULL, "newrank", i);
			gda_set_set_holder_value (params, NULL, "rank", g_value_get_int (rank));
			if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
				g_object_unref (stmt);
				g_object_unref (model);
				goto err;
			}
		}
	}
	if (stmt)
		g_object_unref (stmt);
	stmt = NULL;
	g_object_unref (model);
	if (pos > nrows)
		pos = nrows;
	gda_set_set_holder_value (params, NULL, "rank", pos);

	/* SHIFT favorites */
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_SHIFT, NULL, error);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_SHIFT);
		goto err;
	}
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto err;
	}
	g_object_unref (stmt);
	stmt = NULL;

	/* ADD new favorite */
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_INSERT, NULL, error);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_INSERT);
		goto err;
	}
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto err;
	}
	g_object_unref (stmt);
	stmt = NULL;
					    
	if (! gda_connection_commit_transaction (store_cnc, NULL, NULL)) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't commit transaction to access favorites"));
		goto err;
	}

	if (params)
		g_object_unref (params);
	if (parser)
		g_object_unref (parser);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	g_signal_emit (bcnc, browser_connection_signals [FAV_CHANGED],
		       g_quark_from_string (favorite_type_to_string (fav->type)));
	return TRUE;

 err:
	if (params)
		g_object_unref (params);
	if (parser)
		g_object_unref (parser);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	gda_connection_rollback_transaction (store_cnc, NULL, NULL);
	return FALSE;
}

/**
 * browser_connection_free_favorites_list
 */
void
browser_connection_free_favorites_list (GSList *fav_list)
{
	GSList *list;
	if (!fav_list)
		return;
	for (list = fav_list; list; list = list->next) {
		BrowserConnectionFavorite *fav = (BrowserConnectionFavorite*) list->data;
		g_free (fav->contents);
		g_free (fav->descr);
		g_free (fav);
	}
	g_slist_free (fav_list);
}

/**
 * browser_connection_list_favorites
 *
 * Returns: a new list of #BrowserConnectionFavorite pointers. The list has to 
 *          be freed using browser_connection_free_favorites_list()
 */
GSList *
browser_connection_list_favorites (BrowserConnection *bcnc, guint session_id, BrowserFavoritesType type, GError **error)
{
	GdaMetaStore *mstore;
	GdaConnection *store_cnc;
	GdaStatement *stmt;
	GdaSqlParser *parser = NULL;
	GdaSet *params = NULL;
	GSList *fav_list = NULL;
	GdaDataModel *model;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	if (! bcnc->priv->meta_store_addons_init_done &&
	    ! meta_store_addons_init (bcnc, NULL))
		return NULL;

	mstore = gda_connection_get_meta_store (bcnc->priv->cnc);
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, 0, 0, "%s",
			     _("Can't initialize transaction to access favorites"));
		return NULL;
	}

	/* create parser */
	parser = gda_connection_create_parser (store_cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	/* SELECT favorites */
	params = gda_set_new_inline (2,
				     "session", G_TYPE_INT, session_id,
				     "type", G_TYPE_STRING, favorite_type_to_string (type));
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_SELECT, NULL, NULL);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_SELECT);
		goto out;
	}
	model = gda_connection_statement_execute_select (store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model)
		goto out;
	
	gint i, nrows;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *contents, *descr;

		contents = gda_data_model_get_value_at (model, 0, i, error);
		if (contents)
			descr = gda_data_model_get_value_at (model, 1, i, error);
		if (contents && descr) {
			BrowserConnectionFavorite *fav;
			fav = g_new0 (BrowserConnectionFavorite, 1);
			fav->type = type;
			if (G_VALUE_TYPE (descr) == G_TYPE_STRING)
				fav->descr = g_value_dup_string (descr);
			fav->contents = g_value_dup_string (contents);
			fav_list = g_slist_prepend (fav_list, fav);
		}
		else {
			browser_connection_free_favorites_list (fav_list);
			fav_list = NULL;
			goto out;
		}
	}

 out:
	g_object_unref (parser);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	return g_slist_reverse (fav_list);
}


/**
 * browser_connection_delete_favorite
 */
gboolean
browser_connection_delete_favorite (BrowserConnection *bcnc, guint session_id,
				    BrowserConnectionFavorite *fav, GError **error)
{
	GdaMetaStore *mstore;
	GdaConnection *store_cnc;
	GdaStatement *stmt;
	GdaSqlParser *parser = NULL;
	GdaSet *params = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);
	if (! bcnc->priv->meta_store_addons_init_done &&
	    ! meta_store_addons_init (bcnc, NULL))
		return NULL;

	mstore = gda_connection_get_meta_store (bcnc->priv->cnc);
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	
	gda_lockable_lock (GDA_LOCKABLE (store_cnc));

	/* create parser */
	parser = gda_connection_create_parser (store_cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	/* DELETE any favorite existing */
	params = gda_set_new_inline (3,
				     "session", G_TYPE_INT, session_id,
				     "type", G_TYPE_STRING, favorite_type_to_string (fav->type),
				     "contents", G_TYPE_STRING, fav->contents);
	stmt = gda_sql_parser_parse_string (parser, FAVORITE_DELETE, NULL, error);
	if (!stmt) {
		g_warning ("Could not parse internal statement: %s", FAVORITE_DELETE);
		goto out;
	}
	if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto out;
	}
	g_object_unref (stmt);
	stmt = NULL;
	retval = TRUE;
	g_signal_emit (bcnc, browser_connection_signals [FAV_CHANGED],
		       g_quark_from_string (favorite_type_to_string (fav->type)));

 out:
	g_object_unref (parser);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	return retval;
}
