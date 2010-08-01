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
#include "ui-spec-editor.h"
#include "data-source.h"
#include <libgda/libgda.h>
#include "../support.h"

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

struct _UiSpecEditorPrivate {
	DataSourceManager *mgr;
	
	/* warnings */
	GtkWidget  *info;
};

static void ui_spec_editor_class_init (UiSpecEditorClass *klass);
static void ui_spec_editor_init       (UiSpecEditor *sped, UiSpecEditorClass *klass);
static void ui_spec_editor_dispose    (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * UiSpecEditor class implementation
 */
static void
ui_spec_editor_class_init (UiSpecEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ui_spec_editor_dispose;
}


static void
ui_spec_editor_init (UiSpecEditor *sped, UiSpecEditorClass *klass)
{
	g_return_if_fail (IS_UI_SPEC_EDITOR (sped));

	/* allocate private structure */
	sped->priv = g_new0 (UiSpecEditorPrivate, 1);
}

GType
ui_spec_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (UiSpecEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ui_spec_editor_class_init,
			NULL,
			NULL,
			sizeof (UiSpecEditor),
			0,
			(GInstanceInitFunc) ui_spec_editor_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "UiSpecEditor", &info, 0);
	}
	return type;
}

static void
ui_spec_editor_dispose (GObject *object)
{
	UiSpecEditor *sped = (UiSpecEditor*) object;
	if (sped->priv) {
		if (sped->priv->mgr)
			g_object_unref (sped->priv->mgr);

		g_free (sped->priv);
		sped->priv = NULL;
	}
	parent_class->dispose (object);
}


/**
 * ui_spec_editor_new
 *
 * Returns: the newly created editor.
 */
GtkWidget *
ui_spec_editor_new (DataSourceManager *mgr)
{
	UiSpecEditor *sped;
	GtkWidget *label;

	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);

	sped = g_object_new (UI_SPEC_EDITOR_TYPE, NULL);
	sped->priv->mgr = g_object_ref (mgr);

	label = gtk_label_new ("TODO");
	gtk_box_pack_start (GTK_BOX (sped), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* warning */
	sped->priv->info = NULL;

	return (GtkWidget*) sped;
}
