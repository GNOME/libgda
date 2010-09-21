/* gdaui-entry-none.c
 *
 * Copyright (C) 2003 - 2010 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gdaui-entry-none.h"
#include <libgda/gda-data-handler.h>
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_none_class_init (GdauiEntryNoneClass * class);
static void gdaui_entry_none_init (GdauiEntryNone *srv);
static void gdaui_entry_none_dispose (GObject *object);
static void gdaui_entry_none_finalize (GObject *object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue  *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryNonePrivate
{
	GValue *stored_value; /* this value is never modified */
};


GType
gdaui_entry_none_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryNoneClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_none_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryNone),
			0,
			(GInstanceInitFunc) gdaui_entry_none_init
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryNone", &info, 0);
	}
	return type;
}

static void
gdaui_entry_none_class_init (GdauiEntryNoneClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_none_dispose;
	object_class->finalize = gdaui_entry_none_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->can_expand = can_expand;
}

static void
gdaui_entry_none_init (GdauiEntryNone * entry)
{
	entry->priv = g_new0 (GdauiEntryNonePrivate, 1);
	entry->priv->stored_value = NULL;
}

/**
 * gdaui_entry_none_new:
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_none_new (GType type)
{
	GObject *obj;
	GdauiEntryNone *entry;

	g_return_val_if_fail ((type == G_TYPE_INVALID) || (type != GDA_TYPE_NULL) , NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_NONE, NULL);
	entry = GDAUI_ENTRY_NONE (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (entry), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_none_dispose (GObject   * object)
{
	GdauiEntryNone *entry;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NONE (object));

	entry = GDAUI_ENTRY_NONE (object);
	if (entry->priv) {
		if (entry->priv->stored_value) {
			gda_value_free (entry->priv->stored_value);
			entry->priv->stored_value = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_none_finalize (GObject   * object)
{
	GdauiEntryNone *entry;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NONE (object));

	entry = GDAUI_ENTRY_NONE (object);
	if (entry->priv) {
		g_free (entry->priv);
		entry->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *frame, *label;
	GdauiEntryNone *entry;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap), NULL);
	entry = GDAUI_ENTRY_NONE (mgwrap);
	g_return_val_if_fail (entry->priv, NULL);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	
	label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

	gtk_container_add (GTK_CONTAINER (frame), label);
	gtk_widget_show (label);

	return frame;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryNone *entry;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap));
	entry = GDAUI_ENTRY_NONE (mgwrap);
	g_return_if_fail (entry->priv);

	if (entry->priv->stored_value) {
		gda_value_free (entry->priv->stored_value);
		entry->priv->stored_value = NULL;
	}

	if (value)
		entry->priv->stored_value = gda_value_copy ((GValue *) value);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryNone *entry;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap), NULL);
	entry = GDAUI_ENTRY_NONE (mgwrap);
	g_return_val_if_fail (entry->priv, NULL);

	if (entry->priv->stored_value)
		return gda_value_copy (entry->priv->stored_value);
	else
		return gda_value_new_null ();
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryNone *entry;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap));
	entry = GDAUI_ENTRY_NONE (mgwrap);
	g_return_if_fail (entry->priv);
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	return FALSE;
}
