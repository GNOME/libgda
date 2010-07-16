/* gdaui-entry-wrapper.c
 *
 * Copyright (C) 2003 - 2009 Vivien Malerba
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

#include "gdaui-entry-wrapper.h"
#include <libgda/gda-data-handler.h>
#include <libgda/gda-enums.h>

static void gdaui_entry_wrapper_class_init (GdauiEntryWrapperClass *klass);
static void gdaui_entry_wrapper_init (GdauiEntryWrapper *wid);
static void gdaui_entry_wrapper_dispose (GObject *object);

static void gdaui_entry_wrapper_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gdaui_entry_wrapper_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static void contents_changed_cb (GtkWidget *entry, GdauiEntryWrapper *mgwrap);
static void contents_activated_cb (GtkWidget *entry, GdauiEntryWrapper *mgwrap);
static void check_correct_init (GdauiEntryWrapper *wid);
static void block_signals (GdauiEntryWrapper *wid);
static void unblock_signals (GdauiEntryWrapper *wid);

/* GdauiDataEntry interface */
static void            gdaui_entry_wrapper_data_entry_init   (GdauiDataEntryIface *iface);
static void            gdaui_entry_wrapper_set_value_type    (GdauiDataEntry *de, GType type);
static GType           gdaui_entry_wrapper_get_value_type    (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_set_value         (GdauiDataEntry *de, const GValue *value);
static GValue         *gdaui_entry_wrapper_get_value         (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_set_ref_value     (GdauiDataEntry *de, const GValue *value);
static const GValue   *gdaui_entry_wrapper_get_ref_value     (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_set_value_default (GdauiDataEntry *de, const GValue *value);
static void            gdaui_entry_wrapper_set_attributes    (GdauiDataEntry *de, GdaValueAttribute attrs, guint mask);
static GdaValueAttribute gdaui_entry_wrapper_get_attributes  (GdauiDataEntry *de);
static GdaDataHandler *gdaui_entry_wrapper_get_handler       (GdauiDataEntry *de);
static gboolean        gdaui_entry_wrapper_can_expand        (GdauiDataEntry *de, gboolean horiz);
static void            gdaui_entry_wrapper_set_editable      (GdauiDataEntry *de, gboolean editable);
static gboolean        gdaui_entry_wrapper_get_editable      (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_grab_focus        (GdauiDataEntry *de);

/* properties */
enum {
	PROP_0,
	PROP_SET_DEFAULT_IF_INVALID
};

struct  _GdauiEntryWrapperPriv {
	gboolean                  impl_is_correct;
        GtkWidget                *entry;
	GdauiEntryWrapperClass   *real_class;
	guint                     signals_blocked;

	GType                     type;
	GValue                   *value_ref;
	GValue                   *value_default; /* Can be of any type, not just 'type' */

	gboolean                  null_forced;
	gboolean                  default_forced;

	gboolean                  null_possible;
	gboolean                  default_possible;
	gboolean                  show_actions;
	gboolean                  editable;

	/* property */
	gboolean                  set_default_if_invalid;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gdaui_entry_wrapper_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryWrapperClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_wrapper_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryWrapper),
			0,
			(GInstanceInitFunc) gdaui_entry_wrapper_init
		};

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gdaui_entry_wrapper_data_entry_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDAUI_TYPE_ENTRY_SHELL, "GdauiEntryWrapper", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_ENTRY, &data_entry_info);
	}
	return type;
}

static void
gdaui_entry_wrapper_data_entry_init (GdauiDataEntryIface *iface)
{
	iface->set_value_type = gdaui_entry_wrapper_set_value_type;
	iface->get_value_type = gdaui_entry_wrapper_get_value_type;
	iface->set_value = gdaui_entry_wrapper_set_value;
	iface->get_value = gdaui_entry_wrapper_get_value;
	iface->set_ref_value = gdaui_entry_wrapper_set_ref_value;
	iface->get_ref_value = gdaui_entry_wrapper_get_ref_value;
	iface->set_value_default = gdaui_entry_wrapper_set_value_default;
	iface->set_attributes = gdaui_entry_wrapper_set_attributes;
	iface->get_attributes = gdaui_entry_wrapper_get_attributes;
	iface->get_handler = gdaui_entry_wrapper_get_handler;
	iface->can_expand = gdaui_entry_wrapper_can_expand;
	iface->set_editable = gdaui_entry_wrapper_set_editable;
	iface->get_editable = gdaui_entry_wrapper_get_editable;
	iface->grab_focus = gdaui_entry_wrapper_grab_focus;
}


static void
gdaui_entry_wrapper_class_init (GdauiEntryWrapperClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	klass->create_entry = NULL;
	klass->real_set_value = NULL;
	klass->real_get_value = NULL;

	/* Properties */
        object_class->set_property = gdaui_entry_wrapper_set_property;
        object_class->get_property = gdaui_entry_wrapper_get_property;
        g_object_class_install_property (object_class, PROP_SET_DEFAULT_IF_INVALID,
					 g_param_spec_boolean ("set-default-if-invalid", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->dispose = gdaui_entry_wrapper_dispose;
}

static void
check_correct_init (GdauiEntryWrapper *mgwrap)
{
	if (!mgwrap->priv->impl_is_correct) {
		GtkWidget *entry = NULL;
		GdauiEntryWrapperClass *klass;
		gboolean class_impl_error = FALSE;;

		klass = GDAUI_ENTRY_WRAPPER_CLASS (G_OBJECT_GET_CLASS (mgwrap));
		if (! klass->create_entry) {
			g_warning ("create_entry () virtual function not implemented for object class %s\n",
				   G_OBJECT_TYPE_NAME (mgwrap));
			class_impl_error = TRUE;
		}
		if (! klass->real_set_value) {
			g_warning ("real_set_value () virtual function not implemented for object class %s\n",
				   G_OBJECT_TYPE_NAME (mgwrap));
			class_impl_error = TRUE;
		}
		if (! klass->real_get_value) {
			g_warning ("real_get_value () virtual function not implemented for object class %s\n",
				   G_OBJECT_TYPE_NAME (mgwrap));
			class_impl_error = TRUE;
		}
		if (! klass->connect_signals) {
			g_warning ("connect_signals () virtual function not implemented for object class %s\n",
				   G_OBJECT_TYPE_NAME (mgwrap));
			class_impl_error = TRUE;
		}
		if (! klass->can_expand) {
			g_warning ("can_expand () virtual function not implemented for object class %s\n",
				   G_OBJECT_TYPE_NAME (mgwrap));
			class_impl_error = TRUE;
		}

		if (!class_impl_error) {
			mgwrap->priv->real_class = klass;
			mgwrap->priv->impl_is_correct = TRUE;
			entry = (*mgwrap->priv->real_class->create_entry) (mgwrap);

			gdaui_entry_shell_pack_entry (GDAUI_ENTRY_SHELL (mgwrap), entry);
			gtk_widget_show (entry);
			mgwrap->priv->entry = entry;

			(*mgwrap->priv->real_class->connect_signals) (mgwrap, G_CALLBACK (contents_changed_cb),
								      G_CALLBACK (contents_activated_cb));
		}
		else {
			/* we need to exit because the program WILL BE unstable and WILL crash */
			g_assert_not_reached ();
		}
	}
}

static void
block_signals (GdauiEntryWrapper *mgwrap)
{
	mgwrap->priv->signals_blocked ++;
}

static void
unblock_signals (GdauiEntryWrapper *mgwrap)
{
	mgwrap->priv->signals_blocked --;
}


static void
gdaui_entry_wrapper_init (GdauiEntryWrapper *mgwrap)
{
	/* Private structure */
	mgwrap->priv = g_new0 (GdauiEntryWrapperPriv, 1);
	mgwrap->priv->impl_is_correct = FALSE;
	mgwrap->priv->entry = NULL;
	mgwrap->priv->real_class = NULL;
	mgwrap->priv->signals_blocked = 0;

	mgwrap->priv->type = G_TYPE_INVALID;
	mgwrap->priv->value_ref = NULL;
	mgwrap->priv->value_default = NULL;

	mgwrap->priv->null_forced = FALSE;
	mgwrap->priv->default_forced = FALSE;

	mgwrap->priv->null_possible = TRUE;
	mgwrap->priv->default_possible = FALSE;
	mgwrap->priv->show_actions = TRUE;
	mgwrap->priv->editable = TRUE;

	mgwrap->priv->set_default_if_invalid = FALSE;
}

static void
gdaui_entry_wrapper_dispose (GObject *object)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (object));

	mgwrap = GDAUI_ENTRY_WRAPPER (object);

	if (mgwrap->priv) {
		if (mgwrap->priv->value_ref)
			gda_value_free (mgwrap->priv->value_ref);
		if (mgwrap->priv->value_default)
			gda_value_free (mgwrap->priv->value_default);

		g_free (mgwrap->priv);
		mgwrap->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}


static void
gdaui_entry_wrapper_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdauiEntryWrapper *mgwrap = GDAUI_ENTRY_WRAPPER (object);
	if (mgwrap->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID: {
			guint attrs;

			mgwrap->priv->set_default_if_invalid = g_value_get_boolean (value);
			attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (mgwrap));

			if (mgwrap->priv->set_default_if_invalid && (attrs & GDA_VALUE_ATTR_DATA_NON_VALID)) {
				GValue *sane_value;
				GdaDataHandler *dh;
				GType type;

				check_correct_init (mgwrap);
				dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
				type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap));
				sane_value = gda_data_handler_get_sane_init_value (dh, type);
				(*mgwrap->priv->real_class->real_set_value) (mgwrap, sane_value);
				if (sane_value)
					gda_value_free (sane_value);
			}
			break;
		}
		}
	}
}

static void
gdaui_entry_wrapper_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdauiEntryWrapper *mgwrap = GDAUI_ENTRY_WRAPPER (object);
	if (mgwrap->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			g_value_set_boolean (value, mgwrap->priv->set_default_if_invalid);
			break;
		}
	}
}

/**
 * gdaui_entry_wrapper_contents_changed
 * @mgwrap: a #GdauiEntryWrapper widget
 *
 * Signals to @gwrap that the entry has changed
 */
void
gdaui_entry_wrapper_contents_changed (GdauiEntryWrapper *mgwrap)
{
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (mgwrap));

	contents_changed_cb (NULL, mgwrap);
}

/**
 * gdaui_entry_wrapper_contents_activated
 * @mgwrap: a #GdauiEntryWrapper widget
 *
 * Signals to @gwrap that the entry has been activated (that is the user
 * pressed ENTER for example to signify he has finished entering data)
 */
void
gdaui_entry_wrapper_contents_activated (GdauiEntryWrapper *mgwrap)
{
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (mgwrap));

	contents_activated_cb (NULL, mgwrap);
}


static void gdaui_entry_wrapper_emit_signal (GdauiEntryWrapper *mgwrap);
static void
contents_changed_cb (GtkWidget *entry, GdauiEntryWrapper *mgwrap)
{
	/* @entry is not used */
	if (! mgwrap->priv->signals_blocked) {
		mgwrap->priv->null_forced = FALSE;
		mgwrap->priv->default_forced = FALSE;
		gdaui_entry_wrapper_emit_signal (mgwrap);
	}
}

static void
contents_activated_cb (GtkWidget *entry, GdauiEntryWrapper *mgwrap)
{
	/* @entry is not used */
	if (! mgwrap->priv->signals_blocked) {
		mgwrap->priv->null_forced = FALSE;
		mgwrap->priv->default_forced = FALSE;
#ifdef debug_signal
		g_print (">> 'CONTENTS_ACTIVATED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (mgwrap), "contents-activated");
#ifdef debug_signal
		g_print ("<< 'CONTENTS_ACTIVATED' from %s\n", __FUNCTION__);
#endif
	}
}

static void
gdaui_entry_wrapper_emit_signal (GdauiEntryWrapper *mgwrap)
{
	if (! mgwrap->priv->signals_blocked) {
#ifdef debug_signal
		g_print (">> 'CONTENTS_MODIFIED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit_by_name (G_OBJECT (mgwrap), "contents-modified");
#ifdef debug_signal
		g_print ("<< 'CONTENTS_MODIFIED' from %s\n", __FUNCTION__);
#endif
	}
}



/* Interface implementation */
static void
gdaui_entry_wrapper_set_value_type (GdauiDataEntry *iface, GType type)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;

	if (mgwrap->priv->type != type) {
		GValue *value;
		GdaDataHandler *dh;

		if (mgwrap->priv->value_ref) {
			gda_value_free (mgwrap->priv->value_ref);
			mgwrap->priv->value_ref = NULL;
		}
		if (mgwrap->priv->value_default) {
			gda_value_free (mgwrap->priv->value_default);
			mgwrap->priv->value_default = NULL;
		}

		mgwrap->priv->type = type;
		mgwrap->priv->value_default = gda_value_new_null ();

		/* Set original value */
		dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
		value = gda_value_new_null ();
		gdaui_entry_wrapper_set_ref_value (GDAUI_DATA_ENTRY (mgwrap), value);
 		gda_value_free (value);
	}
}

static GType
gdaui_entry_wrapper_get_value_type (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *mgwrap;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), G_TYPE_INVALID);
	mgwrap = (GdauiEntryWrapper*) iface;

	return mgwrap->priv->type;
}


static void
gdaui_entry_wrapper_set_value (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	block_signals (mgwrap);
	if (value) {
		g_return_if_fail ((G_VALUE_TYPE ((GValue *) value) == mgwrap->priv->type) ||
				  (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL));
		(*mgwrap->priv->real_class->real_set_value) (mgwrap, value);
		if (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL)
			mgwrap->priv->null_forced = TRUE;
		else
			mgwrap->priv->null_forced = FALSE;
		gdaui_entry_shell_set_unknown ((GdauiEntryShell*) iface, FALSE);
	}
	else {
		gdaui_entry_shell_set_unknown ((GdauiEntryShell*) iface, TRUE);
		(*mgwrap->priv->real_class->real_set_value) (mgwrap, NULL);
		mgwrap->priv->null_forced = TRUE;
	}
	unblock_signals (mgwrap);
	mgwrap->priv->default_forced = FALSE;

	gdaui_entry_wrapper_emit_signal (mgwrap);
}

static GValue *
gdaui_entry_wrapper_get_value (GdauiDataEntry *iface)
{
	GValue *value = NULL;
	GdauiEntryWrapper *mgwrap;
	GdaDataHandler *dh;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), NULL);
	mgwrap = (GdauiEntryWrapper*) iface;

	dh = gdaui_entry_wrapper_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	if (mgwrap->priv->null_forced)
		value = gda_value_new_null ();
	else {
		if (mgwrap->priv->default_forced) {
			if (G_VALUE_TYPE (mgwrap->priv->value_default) == mgwrap->priv->type)
				value = gda_value_copy (mgwrap->priv->value_default);
			else
				value = gda_value_new_null ();
		}
		else {
			check_correct_init (mgwrap);
			value = (mgwrap->priv->real_class->real_get_value) (mgwrap);
		}
	}

	return value;
}

static void
gdaui_entry_wrapper_set_ref_value (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryWrapper *mgwrap;
	gboolean changed = TRUE;
	GValue *evalue;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	/* compare existing value and the one provided as argument */
	if (mgwrap->priv->real_class->value_is_equal_to)
		changed = ! mgwrap->priv->real_class->value_is_equal_to (mgwrap, value);
	else {
		evalue = gdaui_entry_wrapper_get_value (iface);
		if ((!value || (G_VALUE_TYPE (value) == GDA_TYPE_NULL)) &&
		    (!evalue || (G_VALUE_TYPE (evalue) == GDA_TYPE_NULL)))
			changed = FALSE;
		else if (!gda_value_differ ((GValue *) value, evalue))
			changed = FALSE;
		if (evalue)
			gda_value_free (evalue);
	}

	/* get rid on any existing orig value */
	if (mgwrap->priv->value_ref) {
		gda_value_free (mgwrap->priv->value_ref);
		mgwrap->priv->value_ref = NULL;
	}

	/* apply changes, if any */
	if (changed) {
		block_signals (mgwrap);
		gdaui_entry_wrapper_set_value (iface, value);
		unblock_signals (mgwrap);
	}

	if (value) {
		g_return_if_fail ((G_VALUE_TYPE ((GValue *) value) == mgwrap->priv->type) ||
				  (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL));
		mgwrap->priv->value_ref = gda_value_copy ((GValue *) value);
	}
	else
		mgwrap->priv->value_ref = gda_value_new_null ();

	/* signal changes if any */
	if (changed)
		gdaui_entry_wrapper_emit_signal (mgwrap);
}

static const GValue *
gdaui_entry_wrapper_get_ref_value (GdauiDataEntry *iface)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), NULL);
	g_return_val_if_fail (GDAUI_ENTRY_WRAPPER (iface)->priv, NULL);

	return GDAUI_ENTRY_WRAPPER (iface)->priv->value_ref;
}

static void
gdaui_entry_wrapper_set_value_default (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;

	if (mgwrap->priv->value_default)
		gda_value_free (mgwrap->priv->value_default);

	if (value)
		mgwrap->priv->value_default = gda_value_copy ((GValue *) value);
	else
		mgwrap->priv->value_default = gda_value_new_null ();

	if (mgwrap->priv->default_forced) {
		if (G_VALUE_TYPE (mgwrap->priv->value_default) == mgwrap->priv->type) {
			check_correct_init (mgwrap);
			block_signals (mgwrap);
			gdaui_entry_wrapper_set_value (iface, mgwrap->priv->value_default);
			unblock_signals (mgwrap);
			mgwrap->priv->default_forced = TRUE;
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
		else {
			check_correct_init (mgwrap);
			(*mgwrap->priv->real_class->real_set_value) (mgwrap, NULL);
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
	}
}

static void
gdaui_entry_wrapper_set_attributes (GdauiDataEntry *iface, GdaValueAttribute attrs, guint mask)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	/* Setting to NULL */
	if (mask & GDA_VALUE_ATTR_IS_NULL) {
		if ((mask & GDA_VALUE_ATTR_CAN_BE_NULL) &&
		    !(attrs & GDA_VALUE_ATTR_CAN_BE_NULL))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_NULL) {
			block_signals (mgwrap);
			gdaui_entry_wrapper_set_value (iface, NULL);
			unblock_signals (mgwrap);
			mgwrap->priv->null_forced = TRUE;

			/* if default is set, see if we can keep it that way */
			if (mgwrap->priv->default_forced) {
				if (G_VALUE_TYPE (mgwrap->priv->value_default) !=
				    GDA_TYPE_NULL)
					mgwrap->priv->default_forced = FALSE;
			}

			gdaui_entry_wrapper_emit_signal (mgwrap);
			return;
		}
		else {
			mgwrap->priv->null_forced = FALSE;
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
	}

	/* Can be NULL ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_NULL)
		mgwrap->priv->null_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE;

	/* Setting to DEFAULT */
	if (mask & GDA_VALUE_ATTR_IS_DEFAULT) {
		if (! (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT))
			g_return_if_reached ();
		if (attrs & GDA_VALUE_ATTR_IS_DEFAULT) {
			block_signals (mgwrap);
			if (mgwrap->priv->value_default) {
				if (G_VALUE_TYPE (mgwrap->priv->value_default) == mgwrap->priv->type)
					gdaui_entry_wrapper_set_value (iface, mgwrap->priv->value_default);
				else
					(*mgwrap->priv->real_class->real_set_value) (mgwrap, NULL);
			}
			else
				gdaui_entry_wrapper_set_value (iface, NULL);
			unblock_signals (mgwrap);

			/* if NULL is set, see if we can keep it that way */
			if (mgwrap->priv->null_forced) {
				if (G_VALUE_TYPE (mgwrap->priv->value_default) !=
				    GDA_TYPE_NULL)
					mgwrap->priv->null_forced = FALSE;
			}

			mgwrap->priv->default_forced = TRUE;
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
		else {
			mgwrap->priv->default_forced = FALSE;
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
	}

	/* Can be DEFAULT ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
		mgwrap->priv->default_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? TRUE : FALSE;

	/* Modified ? */
	if (mask & GDA_VALUE_ATTR_IS_UNCHANGED) {
		if (attrs & GDA_VALUE_ATTR_IS_UNCHANGED) {
			mgwrap->priv->default_forced = FALSE;
			block_signals (mgwrap);
			gdaui_entry_wrapper_set_value (iface, mgwrap->priv->value_ref);
			unblock_signals (mgwrap);
			gdaui_entry_wrapper_emit_signal (mgwrap);
		}
	}

	/* Actions buttons ? */
	if (mask & GDA_VALUE_ATTR_ACTIONS_SHOWN) {
		GValue *gval;
		mgwrap->priv->show_actions = (attrs & GDA_VALUE_ATTR_ACTIONS_SHOWN) ? TRUE : FALSE;

		gval = g_new0 (GValue, 1);
		g_value_init (gval, G_TYPE_BOOLEAN);
		g_value_set_boolean (gval, mgwrap->priv->show_actions);
		g_object_set_property (G_OBJECT (mgwrap), "actions", gval);
		g_free (gval);
	}

	/* NON WRITABLE attributes */
	if (mask & GDA_VALUE_ATTR_DATA_NON_VALID)
		g_warning ("Can't force a GdauiDataEntry to be invalid!");

	if (mask & GDA_VALUE_ATTR_HAS_VALUE_ORIG)
		g_warning ("Having an original value is not a write attribute on GdauiDataEntry!");

	g_signal_emit_by_name (G_OBJECT (mgwrap), "status-changed");
}

static GdaValueAttribute
gdaui_entry_wrapper_get_attributes (GdauiDataEntry *iface)
{
	GdaValueAttribute retval = 0;
	GdauiEntryWrapper *mgwrap;
	GValue *value = NULL;
	gboolean has_current_value;
	gboolean value_is_null = FALSE;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), 0);
	mgwrap = (GdauiEntryWrapper*) iface;

	check_correct_init (mgwrap);
	if (!mgwrap->priv->real_class->value_is_equal_to ||
	    !mgwrap->priv->real_class->value_is_null) {
		value = gdaui_entry_wrapper_get_value (iface);
		has_current_value = TRUE;
	}
	else
		has_current_value = FALSE;

	/* NULL? */
	if (has_current_value) {
		if ((value && (G_VALUE_TYPE (value) == GDA_TYPE_NULL)) || !value) {
			if (mgwrap->priv->default_forced) {
				if (mgwrap->priv->null_forced)
					value_is_null = TRUE;
			}
			else
				value_is_null = TRUE;
		}
	}
	else {
		if ((mgwrap->priv->real_class->value_is_null) (mgwrap))
			value_is_null = TRUE;
	}
	if (value_is_null)
		retval = retval | GDA_VALUE_ATTR_IS_NULL;

	/* can be NULL? */
	if (mgwrap->priv->null_possible)
		retval = retval | GDA_VALUE_ATTR_CAN_BE_NULL;

	/* is default */
	if (mgwrap->priv->default_forced)
		retval = retval | GDA_VALUE_ATTR_IS_DEFAULT;

	/* can be default? */
	if (mgwrap->priv->default_possible)
		retval = retval | GDA_VALUE_ATTR_CAN_BE_DEFAULT;

	/* is unchanged */
	if (has_current_value) {
		if (mgwrap->priv->value_ref &&
		    (G_VALUE_TYPE (value) == G_VALUE_TYPE (mgwrap->priv->value_ref))) {
			if (gda_value_is_null (value))
				retval = retval | GDA_VALUE_ATTR_IS_UNCHANGED;
			else {
				if (! gda_value_differ (value, mgwrap->priv->value_ref))
					retval = retval | GDA_VALUE_ATTR_IS_UNCHANGED;
			}
		}
	}
	else {
		if ((mgwrap->priv->real_class->value_is_equal_to) (mgwrap, mgwrap->priv->value_ref))
			retval = retval | GDA_VALUE_ATTR_IS_UNCHANGED;
	}

	/* actions shown */
	if (mgwrap->priv->show_actions)
		retval = retval | GDA_VALUE_ATTR_ACTIONS_SHOWN;

	/* data valid? */
	if (! (mgwrap->priv->default_forced && mgwrap->priv->default_possible)) {
		if (/*(value_is_null && !mgwrap->priv->null_forced) ||*/
		    (value_is_null && !mgwrap->priv->null_possible))
			retval = retval | GDA_VALUE_ATTR_DATA_NON_VALID;
	}

	/* has original value? */
	if (mgwrap->priv->value_ref)
		retval = retval | GDA_VALUE_ATTR_HAS_VALUE_ORIG;

	if (has_current_value)
		gda_value_free (value);

	if (!mgwrap->priv->editable)
		retval = retval | GDA_VALUE_ATTR_NO_MODIF;

	return retval;
}


static GdaDataHandler *
gdaui_entry_wrapper_get_handler (GdauiDataEntry *iface)
{
	GdaDataHandler *dh;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), NULL);

	g_object_get (G_OBJECT (iface), "handler", &dh, NULL);
	if (dh) /* loose the reference before returning the object */
		g_object_unref (dh);

	return dh;
}

static gboolean
gdaui_entry_wrapper_can_expand (GdauiDataEntry *iface, gboolean horiz)
{
	GdauiEntryWrapper *mgwrap;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), FALSE);
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	return (mgwrap->priv->real_class->can_expand) (mgwrap, horiz);
}

static void
gdaui_entry_wrapper_set_editable (GdauiDataEntry *iface, gboolean editable)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	mgwrap->priv->editable = editable;
	if (mgwrap->priv->real_class->set_editable)
		(mgwrap->priv->real_class->set_editable) (mgwrap, editable);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (iface), editable);
}

static gboolean
gdaui_entry_wrapper_get_editable (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *mgwrap;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), FALSE);
	mgwrap = (GdauiEntryWrapper*) iface;

	return mgwrap->priv->editable;
}

static void
gdaui_entry_wrapper_grab_focus (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *mgwrap;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	mgwrap = (GdauiEntryWrapper*) iface;
	check_correct_init (mgwrap);

	if (mgwrap->priv->real_class->grab_focus)
		(mgwrap->priv->real_class->grab_focus) (mgwrap);
	else if (mgwrap->priv->entry) {
		gboolean canfocus;
		g_object_get ((GObject*) mgwrap->priv->entry, "can-focus", &canfocus, NULL);
		if (canfocus)
			gtk_widget_grab_focus (mgwrap->priv->entry);
	}
}
