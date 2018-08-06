/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "t-virtual-connection.h"
#include <base/base-tool-decl.h>
#include <virtual/libgda-virtual.h>
#include "t-utils.h"
#include "t-app.h"

/* 
 * Main static functions 
 */
static void t_virtual_connection_class_init (TVirtualConnectionClass *klass);
static void t_virtual_connection_init (TVirtualConnection *stmt);
static void t_virtual_connection_dispose (GObject *object);
static void t_virtual_connection_set_property (GObject *object,
						     guint param_id,
						     const GValue *value,
						     GParamSpec *pspec);
static void t_virtual_connection_get_property (GObject *object,
						     guint param_id,
						     GValue *value,
						     GParamSpec *pspec);

struct _TVirtualConnectionPrivate
{
	TVirtualConnectionSpecs *specs;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum {
        PROP_0,
        PROP_SPECS
};

GType
t_virtual_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (TVirtualConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) t_virtual_connection_class_init,
			NULL,
			NULL,
			sizeof (TVirtualConnection),
			0,
			(GInstanceInitFunc) t_virtual_connection_init,
			0
		};

		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (T_TYPE_CONNECTION, "TVirtualConnection", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
source_cnc_busy_cb (G_GNUC_UNUSED TConnection *bcnc, gboolean is_busy,
		    G_GNUC_UNUSED const gchar *reason, TConnection *virtual)
{
	if (t_connection_is_busy (virtual, NULL) != is_busy)
		g_signal_emit_by_name (virtual, "busy", is_busy,
				       is_busy ? _("Bound connection is used") : NULL);
}

static void
m_busy (TConnection *bcnc, gboolean is_busy, const gchar *reason)
{
  g_return_if_fail (T_IS_VIRTUAL_CONNECTION (bcnc));
  g_return_if_fail (T_VIRTUAL_CONNECTION (bcnc)->priv);
	/*
	 * declare all the source connections as busy
	 */
	GSList *list;
	if (! T_VIRTUAL_CONNECTION (bcnc)->priv->specs)
		return;

	for (list = T_VIRTUAL_CONNECTION (bcnc)->priv->specs->parts; list; list = list->next) {
		TVirtualConnectionPart *part;
		part = (TVirtualConnectionPart*) list->data;
		if (part->part_type == T_VIRTUAL_CONNECTION_PART_CNC) {
			TVirtualConnectionCnc *cnc;
			cnc = &(part->u.cnc);
			g_signal_handlers_block_by_func (cnc->source_cnc,
							 G_CALLBACK (source_cnc_busy_cb),
							 bcnc);
			if (t_connection_is_busy (cnc->source_cnc, NULL) != is_busy)
				g_signal_emit_by_name (cnc->source_cnc, "busy", is_busy,
				is_busy ? _("Virtual connection using this connection is busy") : NULL);
			g_signal_handlers_unblock_by_func (cnc->source_cnc,
							   G_CALLBACK (source_cnc_busy_cb),
							   bcnc);			
		}
	}
  if (T_CONNECTION_CLASS (parent_class)->busy != NULL)
    T_CONNECTION_CLASS (parent_class)->busy (bcnc, is_busy, reason);
}

static void
t_virtual_connection_class_init (TVirtualConnectionClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	T_CONNECTION_CLASS (klass)->busy = m_busy;

	/* Properties */
        object_class->set_property = t_virtual_connection_set_property;
        object_class->get_property = t_virtual_connection_get_property;
	g_object_class_install_property (object_class, PROP_SPECS,
                                         g_param_spec_pointer ("specs", NULL,
							       "Specifications as a TVirtualConnectionSpecs pointer",
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	object_class->dispose = t_virtual_connection_dispose;
}

static void
t_virtual_connection_init (TVirtualConnection *bcnc)
{
	bcnc->priv = g_new0 (TVirtualConnectionPrivate, 1);
}

static void
t_virtual_connection_dispose (GObject *object)
{
	TVirtualConnection *bcnc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (T_IS_VIRTUAL_CONNECTION (object));

	bcnc = T_VIRTUAL_CONNECTION (object);

	if (bcnc->priv) {
		if (bcnc->priv->specs) {
			GSList *list;
			for (list = bcnc->priv->specs->parts; list; list = list->next) {
				TVirtualConnectionPart *part;
				part = (TVirtualConnectionPart*) list->data;
				if (part->part_type == T_VIRTUAL_CONNECTION_PART_CNC) {
					TVirtualConnectionCnc *cnc;
					cnc = &(part->u.cnc);
					g_signal_handlers_disconnect_by_func (cnc->source_cnc,
									      G_CALLBACK (source_cnc_busy_cb),
									      bcnc);
				}
			}
			t_virtual_connection_specs_free (bcnc->priv->specs);
		}

		g_free (bcnc->priv);
		bcnc->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
t_virtual_connection_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
        TVirtualConnection *bcnc;

        bcnc = T_VIRTUAL_CONNECTION (object);
        if (bcnc->priv) {
                switch (param_id) {
                case PROP_SPECS:
			bcnc->priv->specs = t_virtual_connection_specs_copy (g_value_get_pointer (value));
			GSList *list;
			for (list = bcnc->priv->specs->parts; list; list = list->next) {
				TVirtualConnectionPart *part;
				part = (TVirtualConnectionPart*) list->data;
				if (part->part_type == T_VIRTUAL_CONNECTION_PART_CNC) {
					TVirtualConnectionCnc *cnc;
					cnc = &(part->u.cnc);
					g_signal_connect (cnc->source_cnc, "busy",
							  G_CALLBACK (source_cnc_busy_cb), bcnc);
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
t_virtual_connection_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
        TVirtualConnection *bcnc;

        bcnc = T_VIRTUAL_CONNECTION (object);
        if (bcnc->priv) {
                switch (param_id) {
                case PROP_SPECS:
			g_value_set_pointer (value, bcnc->priv->specs);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * t_virtual_connection_new
 * @specs: the specifications of the virtual connection's contents
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #TVirtualConnection connection.
 *
 * Returns: (transfert none): the new connection
 */
TConnection *
t_virtual_connection_new (const TVirtualConnectionSpecs *specs, GError **error)
{
#ifdef DUMMY
        sleep (5);
        g_set_error (error, GDA_TOOLS_ERROR, TOOLS_INTERNAL_COMMAND_ERROR,
		     "%s", "Timeout!!!");
        return NULL;
#endif

	/* create GDA virtual connection */
	static GdaVirtualProvider *provider = NULL;
	GdaConnection *virtual;
	if (!provider)
		provider = gda_vprovider_hub_new ();

	virtual = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_AUTO_META_DATA, NULL);
		
	/* add parts to connection as specified by @specs */
	GSList *list;
	for (list = specs->parts; list; list = list->next) {
		TVirtualConnectionPart *part;
		part = (TVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case T_VIRTUAL_CONNECTION_PART_MODEL: {
			TVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
								    pm->model, pm->table_name, error)) {
				g_object_unref (virtual);
				return NULL;
			}
			break;
		}
		case T_VIRTUAL_CONNECTION_PART_CNC: {
			TVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual),
						      t_connection_get_cnc (scnc->source_cnc),
						      scnc->table_schema, error)) {
				g_object_unref (virtual);
				return NULL;
			}
			break;
		}
		default:
			g_assert_not_reached ();
		}
	}

	/* create the TConnection object */
        if (virtual) {
		TConnection *ntcnc;
		ntcnc = g_object_new (T_TYPE_VIRTUAL_CONNECTION, "specs", specs,
				      "gda-connection", virtual, NULL);
		g_object_unref (virtual);
		t_app_add_tcnc (ntcnc);
		return ntcnc;
	}
	else
		return NULL;
}

/**
 * t_virtual_connection_modify_specs
 * @bcnc: a #TVirtualConnection connection
 * @new_specs: a pointer to a #TVirtualConnectionSpecs for the new specs
 * @error: a place to store errors, or %NULL
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
t_virtual_connection_modify_specs (TVirtualConnection *bcnc,
					 const TVirtualConnectionSpecs *new_specs, GError **error)
{
	g_return_val_if_fail (T_IS_VIRTUAL_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (new_specs, FALSE);

	/* undo the current specs */
	GSList *list;
	GdaConnection *cnc;
	cnc = g_object_get_data (G_OBJECT (t_connection_get_cnc (T_CONNECTION (bcnc))),
				 "gda-virtual-connection");
	TO_IMPLEMENT;
	for (list = bcnc->priv->specs->parts; list; list = bcnc->priv->specs->parts) {
		TVirtualConnectionPart *part;
		part = (TVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case T_VIRTUAL_CONNECTION_PART_MODEL: {
			TVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (cnc),
								 pm->table_name, error))
				return FALSE;
			break;
		}
		case T_VIRTUAL_CONNECTION_PART_CNC: {
			TVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_remove (GDA_VCONNECTION_HUB (cnc),
							 t_connection_get_cnc (scnc->source_cnc),
							 error))
				return FALSE;
			break;
		}
		default:
			g_assert_not_reached ();
		}

		/* get rid of @part */
		t_virtual_connection_part_free (part);
		bcnc->priv->specs->parts = g_slist_remove (bcnc->priv->specs->parts, part);
	}

	/* apply the new specs */
	t_virtual_connection_specs_free (bcnc->priv->specs);
	bcnc->priv->specs = g_new0 (TVirtualConnectionSpecs, 1);

	for (list = new_specs->parts; list; list = list->next) {
		TVirtualConnectionPart *part;
		part = (TVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case T_VIRTUAL_CONNECTION_PART_MODEL: {
			TVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc),
								    pm->model, pm->table_name, error))
				return FALSE;
			break;
		}
		case T_VIRTUAL_CONNECTION_PART_CNC: {
			TVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (cnc), t_connection_get_cnc (scnc->source_cnc),
						      scnc->table_schema, error))
				return FALSE;
			break;
		}
		default:
			g_assert_not_reached ();
		}

		/* add @part to bcnc->priv->specs */
		bcnc->priv->specs->parts = g_slist_append (bcnc->priv->specs->parts,
							   t_virtual_connection_part_copy (part));
	}
	
	return TRUE;
}


/*
 * Spec manipulations
 */

/**
 * t_virtual_connection_part_copy
 */
TVirtualConnectionPart *
t_virtual_connection_part_copy (const TVirtualConnectionPart *part)
{
	TVirtualConnectionPart *npart;
	g_return_val_if_fail (part, NULL);

	npart = g_new0 (TVirtualConnectionPart, 1);
	npart->part_type = part->part_type;

	switch (part->part_type) {
	case T_VIRTUAL_CONNECTION_PART_MODEL: {
		const TVirtualConnectionModel *spm;
		TVirtualConnectionModel *npm;
		spm = &(part->u.model);
		npm = &(npart->u.model);
		if (spm->table_name)
			npm->table_name = g_strdup (spm->table_name);
		if (spm->model)
			npm->model = GDA_DATA_MODEL(g_object_ref (G_OBJECT (spm->model)));
		break;
	}
	case T_VIRTUAL_CONNECTION_PART_CNC: {
		const TVirtualConnectionCnc *scnc;
		TVirtualConnectionCnc *ncnc;
		scnc = &(part->u.cnc);
		ncnc = &(npart->u.cnc);
		if (scnc->table_schema)
			ncnc->table_schema = g_strdup (scnc->table_schema);
		if (scnc->source_cnc)
			ncnc->source_cnc = T_CONNECTION(g_object_ref (G_OBJECT (scnc->source_cnc)));
		break;
	}
	default:
		g_assert_not_reached ();
	}

	return npart;
}

/**
 * t_virtual_connection_part_free
 */
void
t_virtual_connection_part_free (TVirtualConnectionPart *part)
{
	if (!part)
		return;

	switch (part->part_type) {
	case T_VIRTUAL_CONNECTION_PART_MODEL: {
		TVirtualConnectionModel *pm;
		pm = &(part->u.model);
		g_free (pm->table_name);
		if (pm->model)
			g_object_unref (pm->model);
		break;
	}
	case T_VIRTUAL_CONNECTION_PART_CNC: {
		TVirtualConnectionCnc *cnc;
		cnc = &(part->u.cnc);
		g_free (cnc->table_schema);
		if (cnc->source_cnc)
			g_object_unref (cnc->source_cnc);
		break;
	}
	default:
		g_assert_not_reached ();
	}
}

/**
 * t_virtual_connection_specs_copy
 */
TVirtualConnectionSpecs *
t_virtual_connection_specs_copy (const TVirtualConnectionSpecs *specs)
{
	TVirtualConnectionSpecs *ns;
	GSList *list;

	g_return_val_if_fail (specs, NULL);

	ns = g_new0 (TVirtualConnectionSpecs, 1);
	for (list = specs->parts; list; list = list->next) {
		TVirtualConnectionPart *npart;
		npart = t_virtual_connection_part_copy ((TVirtualConnectionPart*) list->data);
		ns->parts = g_slist_prepend (ns->parts, npart);
	}
	ns->parts = g_slist_reverse (ns->parts);

	return ns;
}

/**
 * t_virtual_connection_specs_free
 */
void
t_virtual_connection_specs_free (TVirtualConnectionSpecs *specs)
{
	if (!specs)
		return;
	g_slist_foreach (specs->parts, (GFunc) t_virtual_connection_part_free, NULL);
	g_slist_free (specs->parts);
	g_free (specs);
}
