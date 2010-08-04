/*
 * Copyright (C) 2010 Vivien Malerba
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
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-sql-builder.h>

#include "data-source-manager.h"

/*#define DEBUG_SOURCES_SORT*/

/* signals */
enum {
	CHANGED,
	LAST_SIGNAL
};

gint data_source_manager_signals [LAST_SIGNAL] = { 0 };

/* 
 * Main static functions 
 */
static void data_source_manager_class_init (DataSourceManagerClass *klass);
static void data_source_manager_init (DataSourceManager *mgr);
static void data_source_manager_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _DataSourceManagerPrivate {
	BrowserConnection *bcnc;
	GSList *sources_list;
        GdaSet *params; /* execution params */
};

GType
data_source_manager_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (DataSourceManagerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_source_manager_class_init,
			NULL,
			NULL,
			sizeof (DataSourceManager),
			0,
			(GInstanceInitFunc) data_source_manager_init
		};

		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "DataSourceManager", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
data_source_manager_class_init (DataSourceManagerClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	data_source_manager_signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceManagerClass, changed),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);

	object_class->dispose = data_source_manager_dispose;
}

static void
data_source_manager_init (DataSourceManager *mgr)
{
	mgr->priv = g_new0 (DataSourceManagerPrivate, 1);
}

static void
data_source_manager_dispose (GObject *object)
{
	DataSourceManager *mgr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_SOURCE_MANAGER (object));

	mgr = DATA_SOURCE_MANAGER (object);
	if (mgr->priv) {
		if (mgr->priv->params)
			g_object_unref (mgr->priv->params);

		if (mgr->priv->sources_list) {
			g_slist_foreach (mgr->priv->sources_list, (GFunc) g_object_unref, NULL);
			g_slist_free (mgr->priv->sources_list);
			mgr->priv->sources_list = NULL;
		}

		if (mgr->priv->bcnc)
			g_object_unref (mgr->priv->bcnc);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

/**
 * data_source_manager_new
 * @bcnc:
 *
 * Creates a new #DataSourceManager object
 *
 * Returns: a new object
 */
DataSourceManager*
data_source_manager_new (BrowserConnection *bcnc)
{
	DataSourceManager *mgr;
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	mgr = DATA_SOURCE_MANAGER (g_object_new (DATA_SOURCE_MANAGER_TYPE, NULL));
	mgr->priv->bcnc = g_object_ref (bcnc);

	return mgr;
}

/*
 * Tells if @source1 has an import which can be satisfied by an export in @source2
 * Returns: %TRUE if there is a dependency
 */
static gboolean
source_depends_on (DataSource *source1, DataSource *source2)
{
	GdaSet *import;
	import = data_source_get_import (source1);
	if (!import) {
#ifdef DEBUG_SOURCES_SORT
		g_print ("[%s] has no import\n",
			 data_source_get_title (source1));
#endif
		return FALSE;
	}

	GSList *holders;
	GHashTable *export_columns;
	export_columns = data_source_get_export_columns (source2);
	for (holders = import->holders; holders; holders = holders->next) {
		GdaHolder *holder = (GdaHolder*) holders->data;
		if (GPOINTER_TO_INT (g_hash_table_lookup (export_columns, gda_holder_get_id (holder))) >= 1) {
#ifdef DEBUG_SOURCES_SORT
			g_print ("[%s] ====> [%s]\n",
				 data_source_get_title (source1),
				 data_source_get_title (source2));
#endif
			return TRUE;
		}
	}
#ifdef DEBUG_SOURCES_SORT
	g_print ("[%s] ..... [%s]\n",
		 data_source_get_title (source1),
		 data_source_get_title (source2));
#endif
	return FALSE;
}

/**
 * data_source_manager_add_source
 * @mgr:
 * @source:
 */
void
data_source_manager_add_source (DataSourceManager *mgr, DataSource *source)
{
	g_return_if_fail (IS_DATA_SOURCE_MANAGER (mgr));
	g_return_if_fail (IS_DATA_SOURCE (source));

	g_return_if_fail (! g_slist_find (mgr->priv->sources_list, source));

#ifdef DEBUG_SOURCES_SORT
	g_print ("Adding source [%s]\n", data_source_get_title (source));
#endif
	if (! mgr->priv->sources_list)
		mgr->priv->sources_list = g_slist_append (NULL, g_object_ref (source));
	else {
		GSList *list;
		gint lefti, righti, curi;
		gboolean deperror = FALSE;
		lefti = -1;
		righti = g_slist_length (mgr->priv->sources_list);
		for (curi = 0, list = mgr->priv->sources_list;
		     list;
		     curi++, list = list->next) {
			if (source_depends_on (source, (DataSource*) list->data))
				lefti = MAX (lefti, curi);
			else if (source_depends_on ((DataSource*) list->data, source))
				righti = MIN (righti, curi);
		}
		if (lefti < righti) {
			/*g_print ("\tleft=%d right=%d\n", lefti, righti);*/
			list = g_slist_nth (mgr->priv->sources_list, righti);
			if (list)
				mgr->priv->sources_list = g_slist_insert_before (mgr->priv->sources_list, list,
										 g_object_ref (source));
			else
				mgr->priv->sources_list = g_slist_append (mgr->priv->sources_list,
									  g_object_ref (source));
		}
		else {
			if (lefti == righti) {
				DataSource *sourcei;
				sourcei = (DataSource*) g_slist_nth_data (mgr->priv->sources_list,
									  lefti);
				if (source_depends_on (source, sourcei) &&
				    source_depends_on (sourcei, source)) {
					/* there is an error */
					deperror = TRUE;
					TO_IMPLEMENT;
				}
			}
			if (!deperror) {
				GSList *olist;
#ifdef DEBUG_SOURCES_SORT
				g_print ("Reorganizing sources order\n");
#endif
				olist = g_slist_reverse (mgr->priv->sources_list);
				mgr->priv->sources_list = NULL;
				for (list = olist; list; list = list->next) {
					data_source_manager_add_source (mgr, (DataSource*) list->data);
					g_object_unref ((GObject*) list->data);
				}
				data_source_manager_add_source (mgr, source);
			}
		}
	}
	g_signal_emit (mgr, data_source_manager_signals[CHANGED], 0);

#ifdef DEBUG_SOURCES_SORT
	g_print ("Sources in manager:\n");
#endif
	GSList *list;
	gint i;
	for (i = 0, list = mgr->priv->sources_list; list; list = list->next, i++) {
		DataSource *source = DATA_SOURCE (list->data);
#ifdef DEBUG_SOURCES_SORT
		g_print ("\t %d ... %s\n", i, 
			 data_source_get_title (source));
#endif
	}
#ifdef DEBUG_SOURCES_SORT
	g_print ("\n");
#endif
}

/**
 * data_source_manager_remove_all
 * @mgr:
 */
void
data_source_manager_remove_all (DataSourceManager *mgr)
{
	g_return_if_fail (IS_DATA_SOURCE_MANAGER (mgr));

	if (mgr->priv->sources_list) {
		GSList *list;
		list = mgr->priv->sources_list;
		mgr->priv->sources_list = NULL;
		g_signal_emit (mgr, data_source_manager_signals[CHANGED], 0);

		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}
}

/**
 * data_source_manager_remove_source
 * @mgr:
 * @source:
 */
void
data_source_manager_remove_source (DataSourceManager *mgr, DataSource *source)
{
	g_return_if_fail (IS_DATA_SOURCE_MANAGER (mgr));
	g_return_if_fail (IS_DATA_SOURCE (source));

	g_return_if_fail (g_slist_find (mgr->priv->sources_list, source));
	mgr->priv->sources_list = g_slist_remove (mgr->priv->sources_list, source);
	g_signal_emit (mgr, data_source_manager_signals[CHANGED], 0);
	g_object_unref (source);
}

static void
compute_params (DataSourceManager *mgr)
{
	/* cleanning process */
	if (mgr->priv->params) {
		browser_connection_keep_variables (mgr->priv->bcnc, mgr->priv->params);
		g_object_unref (mgr->priv->params);
        }
        mgr->priv->params = NULL;
	
	/* compute new params */
	if (mgr->priv->sources_list) {
		GSList *list;
		for (list = mgr->priv->sources_list; list; list = list->next) {
			DataSource *source;
			GdaSet *set;
			
			source = DATA_SOURCE (list->data);
			set = data_source_get_import (source);
			if (!set)
				continue;

			GSList *holders;
			gboolean found;
			for (found = FALSE, holders = set->holders; holders; holders = holders->next) {
				GSList *list2;
				for (list2 = mgr->priv->sources_list; list2; list2 = list2->next) {
					if (list2 == list)
						continue;
					GHashTable *export_h;
					export_h = data_source_get_export_columns (DATA_SOURCE (list2->data));
					if (g_hash_table_lookup (export_h, 
					      gda_holder_get_id (GDA_HOLDER (holders->data)))) {
						found = TRUE;
						break;
					}
				}
			}
			if (! found) {
				if (! mgr->priv->params)
					mgr->priv->params = gda_set_copy (set);
				else
					gda_set_merge_with_set (mgr->priv->params, set);
				data_source_set_params (source, mgr->priv->params);
			}
		}
	}

	browser_connection_load_variables (mgr->priv->bcnc, mgr->priv->params);
}


/**
 * data_source_manager_get_params
 * @mgr:
 *
 * Returns: a pointer to an internal #GdaSet, must not be modified
 */
GdaSet *
data_source_manager_get_params (DataSourceManager *mgr)
{
	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);

	compute_params (mgr);
	return mgr->priv->params;
}


/**
 * data_source_manager_get_sources_array
 * @mgr:
 *
 * No ref is actually held by any of these pointers, all refs to DataSource are held by
 * the @sources_list pointers
 *
 * Returns: an array of arrays of pointer to the #DataSource objects
 */
GArray *
data_source_manager_get_sources_array (DataSourceManager *mgr, GError **error)
{
	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);

	GSList *list;
	GArray *array = NULL;
#ifdef DEBUG_SOURCES_SORT
	g_print ("** Creating DataSource arrays\n");
#endif
	for (list = mgr->priv->sources_list; list; list = list->next) {
		DataSource *source;
		source = DATA_SOURCE (g_object_ref (G_OBJECT (list->data)));
#ifdef DEBUG_SOURCES_SORT
		g_print ("Taking into account source [%s]\n",
			 data_source_get_title (source));
#endif

		data_source_should_rerun (source);

		GdaSet *import;
		import = data_source_get_import (source);
		if (!import) {
			if (! array) {
				array = g_array_new (FALSE, FALSE, sizeof (GArray*));
				GArray *subarray = g_array_new (FALSE, FALSE, sizeof (DataSource*));
				g_array_append_val (array, subarray);
				g_array_append_val (subarray, source);
			}
			else {
				GArray *subarray = g_array_index (array, GArray*, 0);
				g_array_append_val (subarray, source);
			}
			continue;
		}
		
		if (array) {
			gint i;
			gboolean dep_found = FALSE;
			for (i = array->len - 1; i >= 0 ; i--) {
				GArray *subarray = g_array_index (array, GArray*, i);
				gint j;
				for (j = 0; j < subarray->len; j++) {
					DataSource *source2 = g_array_index (subarray, DataSource*, j);
					if (source_depends_on (source, source2)) {
						dep_found = TRUE;
						/* add source to column i+1 if not yet present */
						if (i == array->len - 1) {
							GArray *subarray = g_array_new (FALSE, FALSE,
											sizeof (DataSource*));
							g_array_append_val (array, subarray);
							g_array_append_val (subarray, source);
						}
						else {
							gint k;
							GArray *subarray = g_array_index (array, GArray*, i+1);
							for (k = 0; k < subarray->len; k++) {
								DataSource *source3 = g_array_index (subarray,
												     DataSource*,
												     k);
								if (source3 == source)
									break;
							}
							if (k == subarray->len)
								g_array_append_val (subarray, source);
						}
						continue;
					}
				}

				if (dep_found)
					break;
			}
			if (! dep_found) {
				/* add source to column 0 */
				GArray *subarray = g_array_index (array, GArray*, 0);
				g_array_append_val (subarray, source);
			}
		}
		else {
			/* add source to column 0 */
			array = g_array_new (FALSE, FALSE, sizeof (GArray*));
			GArray *subarray = g_array_new (FALSE, FALSE, sizeof (DataSource*));
			g_array_append_val (array, subarray);
			g_array_append_val (subarray, source);
		}
	}

#ifdef DEBUG_SOURCES_SORT
	g_print ("** DataSource arrays is: %p\n", array);
	if (array) {
		gint i;
		for (i = 0; i < array->len; i++) {
			GArray *subarray = g_array_index (array, GArray*, i);
			gint j;
			for (j = 0; j < subarray->len; j++) {
				DataSource *source2 = g_array_index (subarray, DataSource*, j);
				g_print ("   %d.%d => [%s]\n", i, j, data_source_get_title (source2));
			}
		}
	}
	g_print ("** DataSource arrays created\n");
#endif
	return array;
}

/**
 * data_source_manager_destroy_sources_array
 */
void
data_source_manager_destroy_sources_array (GArray *array)
{
	gint i;
	g_return_if_fail (array);
	for (i = 0; i < array->len; i++) {
		GArray *subarray;
		subarray = g_array_index (array, GArray *, i);
		gint j;
		for (j = 0; j < subarray->len; j++) {
			DataSource *source;
			source = g_array_index (subarray, DataSource *, j);
			g_object_unref (source);
		}

		g_array_free (subarray, TRUE);
	}
	g_array_free (array, TRUE);
}

/**
 * data_source_manager_get_browser_cnc
 * @mgr:
 */
BrowserConnection *
data_source_manager_get_browser_cnc (DataSourceManager *mgr)
{
	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);
	return mgr->priv->bcnc;
}
