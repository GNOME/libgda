/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include "gdaui-entry-wrapper.h"
#include <libgda/gda-data-handler.h>
#include <libgda/gda-enums.h>

/*#define DEBUG*/

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

static void contents_changed_cb (GdauiEntryWrapper *wrapper);
static void contents_activated_cb (GdauiEntryWrapper *wrapper);
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
static void            gdaui_entry_wrapper_set_editable      (GdauiDataEntry *de, gboolean editable);
static gboolean        gdaui_entry_wrapper_get_editable      (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_grab_focus        (GdauiDataEntry *de);
static void            gdaui_entry_wrapper_set_unknown_color (GdauiDataEntry *de, gdouble red, gdouble green,
							      gdouble blue, gdouble alpha);

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
	GValue                   *value_default; /* Can be of any type, not just @type */

	gboolean                  null_forced;
	gboolean                  default_forced;

	gboolean                  invalid;
	gboolean                  null_possible;
	gboolean                  default_possible;
	gboolean                  editable;
	gboolean                  contents_has_changed; /* since this variable was reset */

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
			(GInstanceInitFunc) gdaui_entry_wrapper_init,
			0
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
	iface->set_editable = gdaui_entry_wrapper_set_editable;
	iface->get_editable = gdaui_entry_wrapper_get_editable;
	iface->grab_focus = gdaui_entry_wrapper_grab_focus;
	iface->set_unknown_color = gdaui_entry_wrapper_set_unknown_color;
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
check_correct_init (GdauiEntryWrapper *wrapper)
{
	if (!wrapper->priv->impl_is_correct) {
		GtkWidget *entry = NULL;
		GdauiEntryWrapperClass *klass;
		gboolean class_impl_error = FALSE;;

		klass = GDAUI_ENTRY_WRAPPER_CLASS (G_OBJECT_GET_CLASS (wrapper));
		if (! klass->create_entry) {
			g_warning ("create_entry () virtual function not implemented for object class %s",
				   G_OBJECT_TYPE_NAME (wrapper));
			class_impl_error = TRUE;
		}
		if (! klass->real_set_value) {
			g_warning ("real_set_value () virtual function not implemented for object class %s",
				   G_OBJECT_TYPE_NAME (wrapper));
			class_impl_error = TRUE;
		}
		if (! klass->real_get_value) {
			g_warning ("real_get_value () virtual function not implemented for object class %s",
				   G_OBJECT_TYPE_NAME (wrapper));
			class_impl_error = TRUE;
		}
		if (! klass->connect_signals) {
			g_warning ("connect_signals () virtual function not implemented for object class %s",
				   G_OBJECT_TYPE_NAME (wrapper));
			class_impl_error = TRUE;
		}
		if ((klass->value_is_null && !klass->value_is_equal_to) ||
		    (!klass->value_is_null && klass->value_is_equal_to)) {
			g_warning ("value_is_null() and value_is_equal_to() virtuals functions must eithe both be implemented for object class %s, or none must be implemented",
				   G_OBJECT_TYPE_NAME (wrapper));
			class_impl_error = TRUE;
		}

		if (!class_impl_error) {
			wrapper->priv->real_class = klass;
			wrapper->priv->impl_is_correct = TRUE;
			entry = (*wrapper->priv->real_class->create_entry) (wrapper);

			gdaui_entry_shell_pack_entry (GDAUI_ENTRY_SHELL (wrapper), entry);
			gtk_widget_show (entry);
			wrapper->priv->entry = entry;

			(*wrapper->priv->real_class->connect_signals) (wrapper, G_CALLBACK (contents_changed_cb),
								       G_CALLBACK (contents_activated_cb));
		}
		else {
			/* we need to exit because the program WILL BE unstable and WILL crash */
			g_assert_not_reached ();
		}
	}
}

static void
block_signals (GdauiEntryWrapper *wrapper)
{
	wrapper->priv->signals_blocked ++;
}

static void
unblock_signals (GdauiEntryWrapper *wrapper)
{
	wrapper->priv->signals_blocked --;
}


static void
gdaui_entry_wrapper_init (GdauiEntryWrapper *wrapper)
{
	/* Private structure */
	wrapper->priv = g_new0 (GdauiEntryWrapperPriv, 1);
	wrapper->priv->impl_is_correct = FALSE;
	wrapper->priv->entry = NULL;
	wrapper->priv->real_class = NULL;
	wrapper->priv->signals_blocked = 0;

	wrapper->priv->type = GDA_TYPE_NULL;
	wrapper->priv->value_ref = NULL;
	wrapper->priv->value_default = NULL;

	wrapper->priv->null_forced = FALSE;
	wrapper->priv->default_forced = FALSE;

	wrapper->priv->invalid = FALSE;
	wrapper->priv->null_possible = TRUE;
	wrapper->priv->default_possible = FALSE;
	wrapper->priv->editable = TRUE;
	wrapper->priv->contents_has_changed = FALSE;

	wrapper->priv->set_default_if_invalid = FALSE;

	gtk_widget_set_hexpand (GTK_WIDGET (wrapper), TRUE);
}

static void
gdaui_entry_wrapper_dispose (GObject *object)
{
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (object));

	wrapper = GDAUI_ENTRY_WRAPPER (object);

	if (wrapper->priv) {
		if (wrapper->priv->value_ref)
			gda_value_free (wrapper->priv->value_ref);
		if (wrapper->priv->value_default)
			gda_value_free (wrapper->priv->value_default);

		g_free (wrapper->priv);
		wrapper->priv = NULL;
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
	GdauiEntryWrapper *wrapper = GDAUI_ENTRY_WRAPPER (object);
	if (wrapper->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID: {
			guint attrs;

			wrapper->priv->set_default_if_invalid = g_value_get_boolean (value);
			attrs = gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wrapper));

			if (wrapper->priv->set_default_if_invalid && (attrs & GDA_VALUE_ATTR_DATA_NON_VALID)) {
				GValue *sane_value;
				GdaDataHandler *dh;
				GType type;

				check_correct_init (wrapper);
				dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (wrapper));
				type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (wrapper));
				sane_value = gda_data_handler_get_sane_init_value (dh, type);
				(*wrapper->priv->real_class->real_set_value) (wrapper, sane_value);
				if (sane_value)
					gda_value_free (sane_value);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
	GdauiEntryWrapper *wrapper = GDAUI_ENTRY_WRAPPER (object);
	if (wrapper->priv) {
		switch (param_id) {
		case PROP_SET_DEFAULT_IF_INVALID:
			g_value_set_boolean (value, wrapper->priv->set_default_if_invalid);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gdaui_entry_wrapper_contents_changed:
 * @wrapper: a #GdauiEntryWrapper widget
 *
 * Signals to @gwrap that the entry has changed
 */
void
gdaui_entry_wrapper_contents_changed (GdauiEntryWrapper *wrapper)
{
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (wrapper));

	contents_changed_cb (wrapper);
}

/**
 * gdaui_entry_wrapper_contents_activated:
 * @wrapper: a #GdauiEntryWrapper widget
 *
 * Signals to @gwrap that the entry has been activated (that is the user
 * pressed ENTER for example to signify he has finished entering data)
 */
void
gdaui_entry_wrapper_contents_activated (GdauiEntryWrapper *wrapper)
{
	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (wrapper));

	contents_activated_cb (wrapper);
}


static void gdaui_entry_wrapper_emit_signal (GdauiEntryWrapper *wrapper);
static void
contents_changed_cb (GdauiEntryWrapper *wrapper)
{
#ifdef DEBUG
	g_print ("%s(%p)\n", __FUNCTION__, wrapper);
#endif
	if (! wrapper->priv->signals_blocked) {
		wrapper->priv->null_forced = FALSE;
		wrapper->priv->default_forced = FALSE;
		wrapper->priv->contents_has_changed = TRUE;
		gdaui_entry_wrapper_emit_signal (wrapper);
	}
}

static void
contents_activated_cb (GdauiEntryWrapper *wrapper)
{
#ifdef DEBUG
	g_print ("%s(%p)\n", __FUNCTION__, wrapper);
#endif
	if (! wrapper->priv->signals_blocked) {
		wrapper->priv->null_forced = FALSE;
		wrapper->priv->default_forced = FALSE;
		g_signal_emit_by_name (G_OBJECT (wrapper), "contents-activated");
	}
}

static void
gdaui_entry_wrapper_emit_signal (GdauiEntryWrapper *wrapper)
{
	if (! wrapper->priv->signals_blocked)
		g_signal_emit_by_name (G_OBJECT (wrapper), "contents-modified");
	_gdaui_entry_shell_attrs_changed (GDAUI_ENTRY_SHELL (wrapper),
					  gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wrapper)));
}



/* Interface implementation */
static void
gdaui_entry_wrapper_set_value_type (GdauiDataEntry *iface, GType type)
{
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;

	if (wrapper->priv->type != type) {
		GValue *value;

		if (wrapper->priv->value_ref) {
			gda_value_free (wrapper->priv->value_ref);
			wrapper->priv->value_ref = NULL;
		}
		if (wrapper->priv->value_default) {
			gda_value_free (wrapper->priv->value_default);
			wrapper->priv->value_default = NULL;
		}

		wrapper->priv->type = type;
		wrapper->priv->value_default = gda_value_new_null ();

		/* Set original value */
		value = gda_value_new_null ();
		gdaui_entry_wrapper_set_ref_value (GDAUI_DATA_ENTRY (wrapper), value);
 		gda_value_free (value);
	}
}

static GType
gdaui_entry_wrapper_get_value_type (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *wrapper;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), G_TYPE_INVALID);
	wrapper = (GdauiEntryWrapper*) iface;

	return wrapper->priv->type;
}


static void
gdaui_entry_wrapper_set_value (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;
	check_correct_init (wrapper);

	block_signals (wrapper);
	if (value) {
		g_return_if_fail ((G_VALUE_TYPE ((GValue *) value) == wrapper->priv->type) ||
				  (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL));
		(*wrapper->priv->real_class->real_set_value) (wrapper, value);
		if (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL)
			wrapper->priv->null_forced = TRUE;
		else
			wrapper->priv->null_forced = FALSE;
	}
	else {
		(*wrapper->priv->real_class->real_set_value) (wrapper, NULL);
		wrapper->priv->null_forced = TRUE;
	}
	unblock_signals (wrapper);
	wrapper->priv->default_forced = FALSE;
	wrapper->priv->contents_has_changed = FALSE;

	gdaui_entry_wrapper_emit_signal (wrapper);
}

static GValue *
gdaui_entry_wrapper_get_value (GdauiDataEntry *iface)
{
	GValue *value = NULL;
	GdauiEntryWrapper *wrapper;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), NULL);
	wrapper = (GdauiEntryWrapper*) iface;

	if (wrapper->priv->null_forced)
		value = gda_value_new_null ();
	else {
		if (wrapper->priv->default_forced) {
			if (G_VALUE_TYPE (wrapper->priv->value_default) == wrapper->priv->type)
				value = gda_value_copy (wrapper->priv->value_default);
			else
				value = gda_value_new_null ();
		}
		else {
			check_correct_init (wrapper);
			value = (wrapper->priv->real_class->real_get_value) (wrapper);
		}
	}

	return value;
}

static void
gdaui_entry_wrapper_set_ref_value (GdauiDataEntry *iface, const GValue *value)
{
	GdauiEntryWrapper *wrapper;
	gboolean changed = TRUE;
	GValue *evalue;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;
	check_correct_init (wrapper);

	/* compare existing value and the one provided as argument */
	if (wrapper->priv->real_class->value_is_equal_to)
		changed = ! wrapper->priv->real_class->value_is_equal_to (wrapper, value);
	else {
		evalue = gdaui_entry_wrapper_get_value (iface);
		g_assert (evalue);
		if ((!value || (G_VALUE_TYPE (value) == GDA_TYPE_NULL)) &&
		    (G_VALUE_TYPE (evalue) == GDA_TYPE_NULL))
			changed = FALSE;
		else if (!gda_value_differ ((GValue *) value, evalue))
			changed = FALSE;
		gda_value_free (evalue);
	}

	/* get rid on any existing orig value */
	if (wrapper->priv->value_ref) {
		gda_value_free (wrapper->priv->value_ref);
		wrapper->priv->value_ref = NULL;
	}

	/* apply changes, if any */
	if (changed) {
		block_signals (wrapper);
		gdaui_entry_wrapper_set_value (iface, value);
		unblock_signals (wrapper);
	}

	if (value) {
		g_return_if_fail ((G_VALUE_TYPE ((GValue *) value) == wrapper->priv->type) ||
				  (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_NULL));
		wrapper->priv->value_ref = gda_value_copy ((GValue *) value);
	}
	else
		wrapper->priv->value_ref = gda_value_new_null ();

	/* signal changes if any */
	if (changed)
		gdaui_entry_wrapper_emit_signal (wrapper);
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
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;

	if (wrapper->priv->value_default)
		gda_value_free (wrapper->priv->value_default);

	if (value)
		wrapper->priv->value_default = gda_value_copy ((GValue *) value);
	else
		wrapper->priv->value_default = gda_value_new_null ();

	if (wrapper->priv->default_forced) {
		if (G_VALUE_TYPE (wrapper->priv->value_default) == wrapper->priv->type) {
			check_correct_init (wrapper);
			block_signals (wrapper);
			gdaui_entry_wrapper_set_value (iface, wrapper->priv->value_default);
			unblock_signals (wrapper);
			wrapper->priv->default_forced = TRUE;
		}
		else {
			check_correct_init (wrapper);
			(*wrapper->priv->real_class->real_set_value) (wrapper, NULL);
		}
		gdaui_entry_wrapper_emit_signal (wrapper);
	}
}

static void
gdaui_entry_wrapper_set_attributes (GdauiDataEntry *iface, GdaValueAttribute attrs, guint mask)
{
	GdauiEntryWrapper *wrapper;
	gboolean do_signal = FALSE;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;
	check_correct_init (wrapper);

#ifdef DEBUG
	g_print ("%s (%p): %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", __FUNCTION__, iface,
		 (mask & GDA_VALUE_ATTR_IS_NULL) ? "IS_NULL:" : "",
		 (mask & GDA_VALUE_ATTR_IS_NULL) ? ((attrs & GDA_VALUE_ATTR_IS_NULL) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_CAN_BE_NULL) ? "CAN_BE_NULL:" : "",
		 (mask & GDA_VALUE_ATTR_CAN_BE_NULL) ? ((attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_IS_DEFAULT) ? "IS_DEFAULT:" : "",
		 (mask & GDA_VALUE_ATTR_IS_DEFAULT) ? ((attrs & GDA_VALUE_ATTR_IS_DEFAULT) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? "CAN_BE_DEFAULT:" : "",
		 (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? ((attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_IS_UNCHANGED) ? "IS_UNCHANGED:" : "",
		 (mask & GDA_VALUE_ATTR_IS_UNCHANGED) ? ((attrs & GDA_VALUE_ATTR_IS_UNCHANGED) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_DATA_NON_VALID) ? "NON_VALID:" : "",
		 (mask & GDA_VALUE_ATTR_DATA_NON_VALID) ? ((attrs & GDA_VALUE_ATTR_DATA_NON_VALID) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_READ_ONLY) ? "READ_ONLY:" : "",
		 (mask & GDA_VALUE_ATTR_READ_ONLY) ? ((attrs & GDA_VALUE_ATTR_READ_ONLY) ? "X" : ".") : "",
		 (mask & GDA_VALUE_ATTR_HAS_VALUE_ORIG) ? "HAS_VALUE_ORIG:" : "",
		 (mask & GDA_VALUE_ATTR_HAS_VALUE_ORIG) ? ((attrs & GDA_VALUE_ATTR_HAS_VALUE_ORIG) ? "X" : ".") : "");
#endif

	/* Can be NULL ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_NULL)
		wrapper->priv->null_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE;

	/* Can be DEFAULT ? */
	if (mask & GDA_VALUE_ATTR_CAN_BE_DEFAULT)
		wrapper->priv->default_possible = (attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? TRUE : FALSE;

	/* Setting to NULL */
	if (mask & GDA_VALUE_ATTR_IS_NULL) {
		if (attrs & GDA_VALUE_ATTR_IS_NULL) {
			block_signals (wrapper);
			gdaui_entry_wrapper_set_value (iface, NULL);
			unblock_signals (wrapper);
			wrapper->priv->null_forced = TRUE;

			/* if default is set, see if we can keep it that way */
			if (wrapper->priv->default_forced &&
			    (G_VALUE_TYPE (wrapper->priv->value_default) != GDA_TYPE_NULL))
					wrapper->priv->default_forced = FALSE;
		}
		else
			wrapper->priv->null_forced = FALSE;
		do_signal = TRUE;
	}

	/* Setting to DEFAULT */
	if (mask & GDA_VALUE_ATTR_IS_DEFAULT) {
		if (attrs & GDA_VALUE_ATTR_IS_DEFAULT) {
			if (! wrapper->priv->default_possible) {
				g_warning ("Data entry does not have a default value");
				wrapper->priv->default_forced = FALSE;
			}
			else {
				block_signals (wrapper);
				if (wrapper->priv->value_default) {
					if (G_VALUE_TYPE (wrapper->priv->value_default) == wrapper->priv->type)
						gdaui_entry_wrapper_set_value (iface, wrapper->priv->value_default);
					else
						(*wrapper->priv->real_class->real_set_value) (wrapper, NULL);
				}
				else
					gdaui_entry_wrapper_set_value (iface, NULL);
				unblock_signals (wrapper);

				/* if NULL is set, see if we can keep it that way */
				if (wrapper->priv->null_forced &&
				    (G_VALUE_TYPE (wrapper->priv->value_default) != GDA_TYPE_NULL))
					wrapper->priv->null_forced = FALSE;
				wrapper->priv->default_forced = TRUE;
			}
		}
		else
			wrapper->priv->default_forced = FALSE;

		do_signal = TRUE;
	}

	/* Reset value to original */
	if (mask & GDA_VALUE_ATTR_IS_UNCHANGED) {
		if (attrs & GDA_VALUE_ATTR_IS_UNCHANGED) {
			wrapper->priv->default_forced = FALSE;
			block_signals (wrapper);
			gdaui_entry_wrapper_set_value (iface, wrapper->priv->value_ref);
			unblock_signals (wrapper);
			do_signal = TRUE;
		}
	}

	/* invalid data */
	if (mask & GDA_VALUE_ATTR_DATA_NON_VALID)
		wrapper->priv->invalid = attrs & GDA_VALUE_ATTR_DATA_NON_VALID;

	/* editable */
	if (mask & GDA_VALUE_ATTR_READ_ONLY)
		gdaui_entry_wrapper_set_editable (GDAUI_DATA_ENTRY (wrapper), attrs & GDA_VALUE_ATTR_READ_ONLY);

	/* NON WRITABLE attributes */
	if (mask & GDA_VALUE_ATTR_HAS_VALUE_ORIG)
		g_warning ("Use gdaui_entry_wrapper_set_value_default() instead of GDA_VALUE_ATTR_HAS_VALUE_ORIG");

	_gdaui_entry_shell_attrs_changed (GDAUI_ENTRY_SHELL (wrapper),
					  gdaui_data_entry_get_attributes (GDAUI_DATA_ENTRY (wrapper)));

	if (do_signal) {
		wrapper->priv->contents_has_changed = FALSE;
		gdaui_entry_wrapper_emit_signal (wrapper);
	}
	g_signal_emit_by_name (G_OBJECT (wrapper), "status-changed");
}

static GdaValueAttribute
gdaui_entry_wrapper_get_attributes (GdauiDataEntry *iface)
{
	GdaValueAttribute retval = 0;
	GdauiEntryWrapper *wrapper;
	GValue *value = NULL;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), 0);
	wrapper = (GdauiEntryWrapper*) iface;

	check_correct_init (wrapper);

	gboolean specific_impl = TRUE;
	if (!wrapper->priv->real_class->value_is_equal_to ||
	    !wrapper->priv->real_class->value_is_null) {
		value = gdaui_entry_wrapper_get_value (iface); /* @value may NOT be @NULL ! */
		g_assert (value);
		specific_impl = FALSE;
	}

	/* can be NULL? */
	if (wrapper->priv->null_possible)
		retval |= GDA_VALUE_ATTR_CAN_BE_NULL;

	/* can be default? */
	if (wrapper->priv->default_possible)
		retval |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;

	/* NULL? */
	gboolean value_is_null = FALSE;
	if (specific_impl) {
		if ((wrapper->priv->real_class->value_is_null) (wrapper))
			value_is_null = TRUE;
	}
	else {
		if (G_VALUE_TYPE (value) == GDA_TYPE_NULL) {
			if (wrapper->priv->default_forced) {
				if (wrapper->priv->null_forced)
					value_is_null = TRUE;
			}
			else
				value_is_null = TRUE;
		}
	}
	if (value_is_null)
		retval |= GDA_VALUE_ATTR_IS_NULL;

	/* is default */
	if (wrapper->priv->default_forced)
		retval |= GDA_VALUE_ATTR_IS_DEFAULT;
	else {
		/* is unchanged */
		if (wrapper->priv->value_ref) {
			if (specific_impl) {
				if ((wrapper->priv->real_class->value_is_equal_to) (wrapper, wrapper->priv->value_ref))
					retval |= GDA_VALUE_ATTR_IS_UNCHANGED;
			}
			else if (G_VALUE_TYPE (value) == G_VALUE_TYPE (wrapper->priv->value_ref)) {
				if (! gda_value_differ (value, wrapper->priv->value_ref))
					retval |= GDA_VALUE_ATTR_IS_UNCHANGED;
			}
		}
	}

	/* data valid? */
	if (wrapper->priv->invalid ||
	    (value_is_null && !wrapper->priv->null_possible))
		retval |= GDA_VALUE_ATTR_DATA_NON_VALID;

	/* has original value? */
	if (wrapper->priv->value_ref)
		retval |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;

	gda_value_free (value);

	/* editable ? */
	if (!wrapper->priv->editable)
		retval |= GDA_VALUE_ATTR_READ_ONLY;

#ifdef DEBUG
	g_print ("%s (%p): %s%s%s%s%s%s%s%s\n", __FUNCTION__, iface,
		 (retval & GDA_VALUE_ATTR_IS_NULL) ? "IS_NULL " : "        ",
		 (retval & GDA_VALUE_ATTR_CAN_BE_NULL) ? "CAN_BE_NULL " : "            ",
		 (retval & GDA_VALUE_ATTR_IS_DEFAULT) ? "IS_DEFAULT " : "           ",
		 (retval & GDA_VALUE_ATTR_CAN_BE_DEFAULT) ? "CAN_BE_DEFAULT " : "               ",
		 (retval & GDA_VALUE_ATTR_IS_UNCHANGED) ? "IS_UNCHANGED " : "             ",
		 (retval & GDA_VALUE_ATTR_DATA_NON_VALID) ? "NON_VALID " : "          ",
		 (retval & GDA_VALUE_ATTR_READ_ONLY) ? "READ_ONLY " : "          ",
		 (retval & GDA_VALUE_ATTR_HAS_VALUE_ORIG) ? "HAS_VALUE_ORIG " : "               ");
#endif
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

static void
gdaui_entry_wrapper_set_editable (GdauiDataEntry *iface, gboolean editable)
{
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;
	check_correct_init (wrapper);

	wrapper->priv->editable = editable;
	if (wrapper->priv->real_class->set_editable)
		(wrapper->priv->real_class->set_editable) (wrapper, editable);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (iface), editable);
	_gdaui_entry_shell_mark_editable (GDAUI_ENTRY_SHELL (wrapper), editable);
}

static gboolean
gdaui_entry_wrapper_get_editable (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *wrapper;

	g_return_val_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface), FALSE);
	wrapper = (GdauiEntryWrapper*) iface;

	return wrapper->priv->editable;
}

static void
gdaui_entry_wrapper_grab_focus (GdauiDataEntry *iface)
{
	GdauiEntryWrapper *wrapper;

	g_return_if_fail (GDAUI_IS_ENTRY_WRAPPER (iface));
	wrapper = (GdauiEntryWrapper*) iface;
	check_correct_init (wrapper);

	if (wrapper->priv->real_class->grab_focus)
		(wrapper->priv->real_class->grab_focus) (wrapper);
	else if (wrapper->priv->entry) {
		gboolean canfocus;
		g_object_get ((GObject*) wrapper->priv->entry, "can-focus", &canfocus, NULL);
		if (canfocus)
			gtk_widget_grab_focus (wrapper->priv->entry);
	}
}

static void
gdaui_entry_wrapper_set_unknown_color (GdauiDataEntry *de, gdouble red, gdouble green,
				       gdouble blue, gdouble alpha)
{
	gdaui_entry_shell_set_invalid_color (GDAUI_ENTRY_SHELL (de), red, green, blue, alpha);
}
