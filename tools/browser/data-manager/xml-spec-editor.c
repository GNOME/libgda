/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "xml-spec-editor.h"
#include "data-source.h"
#include <libgda/libgda.h>
#include "../ui-support.h"
#include <libgda/gda-debug-macros.h>
#include <common/t-errors.h>

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksource.h>
#endif

struct _XmlSpecEditorPrivate {
	DataSourceManager *mgr;
	
	/* warnings */
	GtkWidget  *info;
	GtkWidget  *info_label;

	/* XML view */
	gboolean xml_view_up_to_date;
	guint signal_editor_changed_id; /* timout ID to signal editor changed */
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkWidget *help;
};

static void xml_spec_editor_class_init (XmlSpecEditorClass *klass);
static void xml_spec_editor_init       (XmlSpecEditor *sped, XmlSpecEditorClass *klass);
static void xml_spec_editor_dispose    (GObject *object);
static void xml_spec_editor_grab_focus (GtkWidget *widget);

static void source_list_changed_cb (DataSourceManager *mgr, XmlSpecEditor *sped);
static void data_source_changed_cb (DataSourceManager *mgr, DataSource *source, XmlSpecEditor *sped);

static GObjectClass *parent_class = NULL;

/*
 * XmlSpecEditor class implementation
 */
static void
xml_spec_editor_class_init (XmlSpecEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	GTK_WIDGET_CLASS (klass)->grab_focus = xml_spec_editor_grab_focus;
	object_class->dispose = xml_spec_editor_dispose;
}

static void
xml_spec_editor_grab_focus (GtkWidget *widget)
{
	gtk_widget_grab_focus (XML_SPEC_EDITOR (widget)->priv->text);
}

static void
xml_spec_editor_init (XmlSpecEditor *sped, G_GNUC_UNUSED XmlSpecEditorClass *klass)
{
	g_return_if_fail (IS_XML_SPEC_EDITOR (sped));

	/* allocate private structure */
	sped->priv = g_new0 (XmlSpecEditorPrivate, 1);
	sped->priv->signal_editor_changed_id = 0;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (sped), GTK_ORIENTATION_VERTICAL);
}

GType
xml_spec_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (XmlSpecEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) xml_spec_editor_class_init,
			NULL,
			NULL,
			sizeof (XmlSpecEditor),
			0,
			(GInstanceInitFunc) xml_spec_editor_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "XmlSpecEditor", &info, 0);
	}
	return type;
}

static void
xml_spec_editor_dispose (GObject *object)
{
	XmlSpecEditor *sped = (XmlSpecEditor*) object;
	if (sped->priv) {
		if (sped->priv->signal_editor_changed_id)
			g_source_remove (sped->priv->signal_editor_changed_id);
		if (sped->priv->mgr) {
			g_signal_handlers_disconnect_by_func (sped->priv->mgr,
							      G_CALLBACK (source_list_changed_cb), sped);
			g_signal_handlers_disconnect_by_func (sped->priv->mgr,
							      G_CALLBACK (data_source_changed_cb), sped);

			g_object_unref (sped->priv->mgr);
		}

		g_free (sped->priv);
		sped->priv = NULL;
	}
	parent_class->dispose (object);
}

static gboolean
signal_editor_changed (XmlSpecEditor *sped)
{
	GSList *newlist = NULL;
	g_signal_handlers_block_by_func (sped->priv->mgr,
					 G_CALLBACK (source_list_changed_cb), sped);

	/* create new DataSource objects */
	GError *lerror = NULL;
	gchar *xml;
	xmlDocPtr doc = NULL;
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter (sped->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (sped->priv->buffer, &end);
	xml = gtk_text_buffer_get_text (sped->priv->buffer, &start, &end, FALSE);

	if (xml) {
		g_strstrip (xml);
		if (! *xml) {
			g_free (xml);
			goto out;
		}

		doc = xmlParseDoc (BAD_CAST xml);
		g_free (xml);
	}

	if (!doc) {
		TO_IMPLEMENT;
		g_set_error (&lerror, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Error parsing XML specifications"));
		goto out;
	}

	xmlNodePtr node;
	node = xmlDocGetRootElement (doc);
	if (!node) {
		/* nothing to do => finished */
		xmlFreeDoc (doc);
		goto out;
	}

	if (strcmp ((gchar*) node->name, "data")) {
		g_set_error (&lerror, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("Expecting <%s> root node"), "data");
		xmlFreeDoc (doc);
		goto out;
	}

	TConnection *tcnc;
	tcnc = data_source_manager_get_browser_cnc (sped->priv->mgr);
	for (node = node->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "table") ||
		    !strcmp ((gchar*) node->name, "query")) {
			DataSource *source;
			source = data_source_new_from_xml_node (tcnc, node, &lerror);
			if (!source) {
				if (newlist) {
					g_slist_foreach (newlist, (GFunc) g_object_unref, NULL);
					g_slist_free (newlist);
					newlist = NULL;
				}
				xmlFreeDoc (doc);
				goto out;
			}
			else
				newlist = g_slist_prepend (newlist, source);
		}
	}
	xmlFreeDoc (doc);

 out:
	newlist = g_slist_reverse (newlist);
	data_source_manager_replace_all (sped->priv->mgr, newlist);
	if (newlist)
		g_slist_free (newlist);

	if (lerror) {
		if (! sped->priv->info) {
			sped->priv->info = gtk_info_bar_new ();
			gtk_box_pack_start (GTK_BOX (sped), sped->priv->info, FALSE, FALSE, 0);
			sped->priv->info_label = gtk_label_new ("");
			gtk_widget_set_halign (sped->priv->info_label, GTK_ALIGN_START);
			gtk_label_set_ellipsize (GTK_LABEL (sped->priv->info_label), PANGO_ELLIPSIZE_END);
			gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (sped->priv->info))),
					   sped->priv->info_label);
			gtk_widget_show (sped->priv->info_label);
		}
		gchar *str;
		str = g_strdup_printf (_("Error: %s"), lerror->message);
		g_clear_error (&lerror);
		gtk_label_set_text (GTK_LABEL (sped->priv->info_label), str);
		g_free (str);
		gtk_widget_show (sped->priv->info);
	}
	else if (sped->priv->info) {
		gtk_widget_hide (sped->priv->info);
	}

	/* remove timeout */
	sped->priv->signal_editor_changed_id = 0;

	g_signal_handlers_unblock_by_func (sped->priv->mgr,
					   G_CALLBACK (source_list_changed_cb), sped);

	return FALSE;
}

static void
editor_changed_cb (G_GNUC_UNUSED GtkTextBuffer *buffer, XmlSpecEditor *sped)
{
	if (sped->priv->signal_editor_changed_id)
		g_source_remove (sped->priv->signal_editor_changed_id);
	sped->priv->signal_editor_changed_id = g_timeout_add_seconds (1, (GSourceFunc) signal_editor_changed, sped);
}

static void
source_list_changed_cb (G_GNUC_UNUSED DataSourceManager *mgr, XmlSpecEditor *sped)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "data");
	xmlDocSetRootElement (doc, root);

	const GSList *list;
	for (list = data_source_manager_get_sources (sped->priv->mgr); list; list = list->next) {
		xmlNodePtr node;
		node = data_source_to_xml_node (DATA_SOURCE (list->data));
		xmlAddChild (root, node);
	}

	xmlChar *mem;
	int size;
	xmlDocDumpFormatMemory (doc, &mem, &size, 1);
	xmlFreeDoc (doc);

	g_signal_handlers_block_by_func (sped->priv->buffer,
					 G_CALLBACK (editor_changed_cb), sped);
	gtk_text_buffer_set_text (sped->priv->buffer, (gchar*) mem, -1);
	g_signal_handlers_unblock_by_func (sped->priv->buffer,
					   G_CALLBACK (editor_changed_cb), sped);
	xmlFree (mem);
}

static void
data_source_changed_cb (DataSourceManager *mgr, G_GNUC_UNUSED DataSource *source, XmlSpecEditor *sped)
{
	source_list_changed_cb (mgr, sped);
}

/**
 * xml_spec_editor_new
 *
 * Returns: the newly created editor.
 */
GtkWidget *
xml_spec_editor_new (DataSourceManager *mgr)
{
	XmlSpecEditor *sped;
	GtkWidget *sw, *label;
	gchar *str;

	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);

	sped = g_object_new (XML_SPEC_EDITOR_TYPE, NULL);
	sped->priv->mgr = g_object_ref (mgr);
	g_signal_connect (mgr, "list-changed",
			  G_CALLBACK (source_list_changed_cb), sped);
	g_signal_connect (mgr, "source-changed",
			  G_CALLBACK (data_source_changed_cb), sped);

	/* XML editor */
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("SQL code to execute:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (sped), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_OUT);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	gtk_box_pack_start (GTK_BOX (sped), sw, TRUE, TRUE, 0);

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
	gtk_widget_show_all (sw);

	/* warning, not shown */
	sped->priv->info = NULL;

	return (GtkWidget*) sped;
}

/**
 * xml_spec_editor_set_xml_text
 */
void
xml_spec_editor_set_xml_text (XmlSpecEditor *sped, const gchar *xml)
{
	g_return_if_fail (IS_XML_SPEC_EDITOR (sped));

	g_signal_handlers_block_by_func (sped->priv->buffer,
					 G_CALLBACK (editor_changed_cb), sped);
	gtk_text_buffer_set_text (sped->priv->buffer, xml, -1);
	signal_editor_changed (sped);
	g_signal_handlers_unblock_by_func (sped->priv->buffer,
					   G_CALLBACK (editor_changed_cb), sped);
}

/**
 * xml_spec_editor_get_xml_text
 */
gchar *
xml_spec_editor_get_xml_text (XmlSpecEditor *sped)
{
	GtkTextIter start, end;
	g_return_val_if_fail (IS_XML_SPEC_EDITOR (sped), NULL);
	gtk_text_buffer_get_start_iter (sped->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (sped->priv->buffer, &end);

	return gtk_text_buffer_get_text (sped->priv->buffer, &start, &end, FALSE);
}
