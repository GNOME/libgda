/*
 * Copyright (C) 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "spec-editor.h"
#include "data-source.h"
#include <libgda/libgda.h>
#include "../support.h"
#include "marshal.h"

#ifdef HAVE_GTKSOURCEVIEW
  #ifdef GTK_DISABLE_SINGLE_INCLUDES
    #undef GTK_DISABLE_SINGLE_INCLUDES
  #endif

  #include <gtksourceview/gtksourceview.h>
  #include <gtksourceview/gtksourcelanguagemanager.h>
  #include <gtksourceview/gtksourcebuffer.h>
  #include <gtksourceview/gtksourcestyleschememanager.h>
  #include <gtksourceview/gtksourcestylescheme.h>
#endif

struct _SpecEditorPrivate {
	BrowserConnection *bcnc;

	SpecEditorMode mode;
	GtkNotebook *notebook;

        GdaSet *params; /* execution params */

	/* reference for all views */
	xmlDocPtr doc;

	/* XML view */
	gboolean xml_view_up_to_date;
	guint signal_editor_changed_id; /* timout ID to signal editor changed */
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkWidget *help;

	/* UI view */
	gboolean ui_view_up_to_date;
};

static void spec_editor_class_init (SpecEditorClass *klass);
static void spec_editor_init       (SpecEditor *sped, SpecEditorClass *klass);
static void spec_editor_dispose    (GObject *object);

enum {
	CHANGED,
        LAST_SIGNAL
};

static guint spec_editor_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * SpecEditor class implementation
 */
static void
spec_editor_class_init (SpecEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
        spec_editor_signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (SpecEditorClass, changed),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);

	object_class->dispose = spec_editor_dispose;
}


static void
spec_editor_init (SpecEditor *sped, SpecEditorClass *klass)
{
	g_return_if_fail (IS_SPEC_EDITOR (sped));

	/* allocate private structure */
	sped->priv = g_new0 (SpecEditorPrivate, 1);
	sped->priv->signal_editor_changed_id = 0;
	sped->priv->mode = SPEC_EDITOR_XML;
}

GType
spec_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (SpecEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) spec_editor_class_init,
			NULL,
			NULL,
			sizeof (SpecEditor),
			0,
			(GInstanceInitFunc) spec_editor_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "SpecEditor", &info, 0);
	}
	return type;
}

static void
spec_editor_dispose (GObject *object)
{
	SpecEditor *sped = (SpecEditor*) object;
	if (sped->priv) {
		if (sped->priv->signal_editor_changed_id)
			g_source_remove (sped->priv->signal_editor_changed_id);
		if (sped->priv->params)
			g_object_unref (sped->priv->params);
		if (sped->priv->bcnc)
			g_object_unref (sped->priv->bcnc);

		if (sped->priv->doc)
			xmlFreeDoc (sped->priv->doc);
		g_free (sped->priv);
		sped->priv = NULL;
	}
	parent_class->dispose (object);
}

static gboolean
signal_editor_changed (SpecEditor *sped)
{
	/* send signal */
	g_signal_emit (sped, spec_editor_signals[CHANGED], 0);

	/* remove timeout */
	sped->priv->signal_editor_changed_id = 0;
	return FALSE;
}

static void
editor_changed_cb (GtkTextBuffer *buffer, SpecEditor *sped)
{
	if (sped->priv->signal_editor_changed_id)
		g_source_remove (sped->priv->signal_editor_changed_id);
	sped->priv->signal_editor_changed_id = g_timeout_add_seconds (1, (GSourceFunc) signal_editor_changed, sped);
}

/**
 * spec_editor_new
 *
 * Returns: the newly created editor.
 */
SpecEditor *
spec_editor_new (BrowserConnection *bcnc)
{
	SpecEditor *sped;
	GtkWidget *sw, *nb, *exp, *vbox;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);

	sped = g_object_new (SPEC_EDITOR_TYPE, NULL);
	sped->priv->bcnc = g_object_ref (bcnc);

	nb = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (sped), nb, TRUE, TRUE, 0);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	sped->priv->notebook = (GtkNotebook*) nb;

	/* XML editor page */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), vbox, NULL);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_OUT);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

#ifdef HAVE_GTKSOURCEVIEW
        sped->priv->text = gtk_source_view_new ();
        gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (sped->priv->text))),
						TRUE);

	gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (sped->priv->text))),
					gtk_source_language_manager_get_language (gtk_source_language_manager_get_default (),
										  "xml"));
#else
        sped->priv->text = gtk_text_view_new ();
#endif
	gtk_container_add (GTK_CONTAINER (sw), sped->priv->text);	
	sped->priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (sped->priv->text));

	g_signal_connect (sped->priv->buffer, "changed",
			  G_CALLBACK (editor_changed_cb), sped);

	/* UI page */
	GtkWidget *wid;
	wid = gtk_label_new ("TODO");
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, NULL);

	gtk_widget_show_all (nb);

	return SPEC_EDITOR (sped);
}

/**
 * spec_editor_set_xml_text
 */
void
spec_editor_set_xml_text (SpecEditor *sped, const gchar *xml)
{
	g_return_if_fail (IS_SPEC_EDITOR (sped));

	gtk_text_buffer_set_text (sped->priv->buffer, xml, -1);
	signal_editor_changed (sped);
}

/**
 * spec_editor_get_xml_text
 */
gchar *
spec_editor_get_xml_text (SpecEditor *sped)
{
	GtkTextIter start, end;
	g_return_val_if_fail (IS_SPEC_EDITOR (sped), NULL);
	gtk_text_buffer_get_start_iter (sped->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (sped->priv->buffer, &end);

	return gtk_text_buffer_get_text (sped->priv->buffer, &start, &end, FALSE);
}

/**
 * spec_editor_set_mode
 */
void
spec_editor_set_mode (SpecEditor *sped, SpecEditorMode mode)
{
	g_return_if_fail (IS_SPEC_EDITOR (sped));
	switch (mode) {
	case SPEC_EDITOR_XML:
		gtk_notebook_set_current_page (sped->priv->notebook, 0);
		break;
	case SPEC_EDITOR_UI:
		gtk_notebook_set_current_page (sped->priv->notebook, 1);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

/**
 * spec_editor_get_mode
 */
SpecEditorMode
spec_editor_get_mode (SpecEditor *sped)
{
	g_return_val_if_fail (IS_SPEC_EDITOR (sped), SPEC_EDITOR_UI);
	return sped->priv->mode;
}

static GArray *compute_sources (SpecEditor *sped, GError **error);
static GSList *compute_sources_list (SpecEditor *sped, GError **error);

static void
compute_params (SpecEditor *sped)
{
	/* cleanning process */
	if (sped->priv->params) {
		browser_connection_keep_variables (sped->priv->bcnc, sped->priv->params);
		g_object_unref (sped->priv->params);
        }
        sped->priv->params = NULL;
	
	/* compute new params */
	GSList *sources_list;
	sources_list = compute_sources_list (sped, NULL);
	if (sources_list) {
		GSList *list;
		for (list = sources_list; list; list = list->next) {
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
				for (list2 = sources_list; list2; list2 = list2->next) {
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
				if (! sped->priv->params)
					sped->priv->params = gda_set_copy (set);
				else
					gda_set_merge_with_set (sped->priv->params, set);
			}
		}

		/* cleanups */
		g_slist_foreach (sources_list, (GFunc) g_object_unref, NULL);
		g_slist_free (sources_list);
		sources_list = NULL;
	}

	browser_connection_load_variables (sped->priv->bcnc, sped->priv->params);
}

/**
 * spec_editor_get_params
 *
 * Returns: a pointer to an internal #GdaSet, must not be modified
 */
GdaSet *
spec_editor_get_params (SpecEditor *sped)
{
	g_return_val_if_fail (IS_SPEC_EDITOR (sped), NULL);

	compute_params (sped);
	return sped->priv->params;
}

/**
 * spec_editor_get_sources_array
 */
GArray *
spec_editor_get_sources_array (SpecEditor *sped, GError **error)
{
	g_return_val_if_fail (IS_SPEC_EDITOR (sped), NULL);
	if (sped->priv->signal_editor_changed_id > 0) {
		g_source_remove (sped->priv->signal_editor_changed_id);
		sped->priv->signal_editor_changed_id = 0;
		compute_params (sped);
	}
	return compute_sources (sped, error);
}

/**
 * spec_editor_destroy_sources_array
 */
void
spec_editor_destroy_sources_array (GArray *array)
{
	gint i;
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

/*
 * Tells if @source1 has an import which can be satisfied by an export in @source2
 * Returns: %TRUE if there is a dependency
 */
static gboolean
source_depends_on (DataSource *source1, DataSource *source2)
{
	GdaSet *import;
	import = data_source_get_import (source1);
	if (!import)
		return FALSE;

	GSList *holders;
	GHashTable *export_columns;
	export_columns = data_source_get_export_columns (source2);
	for (holders = import->holders; holders; holders = holders->next) {
		GdaHolder *holder = (GdaHolder*) holders->data;
		if (GPOINTER_TO_INT (g_hash_table_lookup (export_columns, gda_holder_get_id (holder))) >= 1)
			return TRUE;
	}
	return FALSE;
}

/*
 * Returns: an array of arrays of pointer to the #DataSource objects
 *
 * No ref is actually held by any of these pointers, all refs to DataSource are held by
 * the @sources_list pointers
 */
static GArray *
create_sources_array (GSList *sources_list, GError **error)
{
	GSList *list;
	GArray *array = NULL;
	g_print ("** Creating DataSource arrays\n");
	for (list = sources_list; list; list = list->next) {
		DataSource *source;
		source = DATA_SOURCE (g_object_ref (G_OBJECT (list->data)));
		g_print ("Taking into account source [%s]\n",
			 data_source_get_title (source));

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
					g_print ("Source [%s] %s on source [%s]\n",
						 data_source_get_title (source),
						 source_depends_on (source, source2) ?
						 "depends" : "does not depend",
						 data_source_get_title (source2));
					if (source_depends_on (source, source2)) {
						dep_found = TRUE;
						/* add source to column i+1 */
						if (i == array->len - 1) {
							GArray *subarray = g_array_new (FALSE, FALSE,
											sizeof (DataSource*));
							g_array_append_val (array, subarray);
							g_array_append_val (subarray, source);
						}
						else {
							GArray *subarray = g_array_index (array, GArray*, i+1);
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

	g_print ("** DataSource arrays created\n");
	return array;
}

static gint
data_source_compare_func (DataSource *s1, DataSource *s2)
{
	if (source_depends_on (s1, s2))
		return 1;
	else if (source_depends_on (s2, s1))
		return -1;
	else
		return 0;
}

static GSList *
compute_sources_list (SpecEditor *sped, GError **error)
{
	gchar *xml;
	xmlDocPtr doc = NULL;
	GSList *sources_list = NULL;

	/* create sources_list from XML */
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter (sped->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (sped->priv->buffer, &end);
	xml = gtk_text_buffer_get_text (sped->priv->buffer, &start, &end, FALSE);
	if (xml) {
		doc = xmlParseDoc (BAD_CAST xml);
		g_free (xml);
	}

	if (!doc) {
		g_set_error (error, 0, 0,
			     _("Error parsing XML specifications"));
		goto onerror;
	}

	xmlNodePtr node;
	node = xmlDocGetRootElement (doc);
	if (!node) {
		/* nothing to do => finished */
		xmlFreeDoc (doc);
		return NULL;
	}

	for (node = node->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "table") ||
		    !strcmp ((gchar*) node->name, "query")) {
			DataSource *source;
			source = data_source_new_from_xml_node (sped->priv->bcnc, node, error);
			if (!source)
				goto onerror;
			
			sources_list = g_slist_prepend (sources_list, source);
			data_source_set_params (source, sped->priv->params);
		}
	}
	xmlFreeDoc (doc);
	doc = NULL;
	return g_slist_sort (sources_list, (GCompareFunc) data_source_compare_func);

 onerror:
	if (doc)
		xmlFreeDoc (doc);
	if (sources_list) {
		g_slist_foreach (sources_list, (GFunc) g_object_unref, NULL);
		g_slist_free (sources_list);
	}
	
	return NULL;
}

static GArray *
compute_sources (SpecEditor *sped, GError **error)
{
	GArray *sources_array = NULL;
	GSList *sources_list;
	GError *lerror = NULL;

	sources_list = compute_sources_list (sped, &lerror);
	if (!lerror)
		sources_array = create_sources_array (sources_list, &lerror);

	if (sources_list) {
		g_slist_foreach (sources_list, (GFunc) g_object_unref, NULL);
		g_slist_free (sources_list);
		sources_list = NULL;
	}

	if (lerror) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) sped),
				    lerror && lerror->message ? lerror->message :_("Error parsing XML specifications"));
		g_clear_error (&lerror);
	}

	return sources_array;
}
