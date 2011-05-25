/*
 * Copyright (C) 2009 - 2011 Vivien Malerba
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
#include "browser-virtual-connection.h"
#include "browser-connection-priv.h"
#include <virtual/libgda-virtual.h>

/* 
 * Main static functions 
 */
static void browser_virtual_connection_class_init (BrowserVirtualConnectionClass *klass);
static void browser_virtual_connection_init (BrowserVirtualConnection *stmt);
static void browser_virtual_connection_dispose (GObject *object);
static void browser_virtual_connection_set_property (GObject *object,
						     guint param_id,
						     const GValue *value,
						     GParamSpec *pspec);
static void browser_virtual_connection_get_property (GObject *object,
						     guint param_id,
						     GValue *value,
						     GParamSpec *pspec);

struct _BrowserVirtualConnectionPrivate
{
	GtkTable    *layout_table;
	BrowserVirtualConnectionSpecs *specs;
	gboolean in_m_busy;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum {
        PROP_0,
        PROP_SPECS
};

GType
browser_virtual_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (BrowserVirtualConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_virtual_connection_class_init,
			NULL,
			NULL,
			sizeof (BrowserVirtualConnection),
			0,
			(GInstanceInitFunc) browser_virtual_connection_init,
			0
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (BROWSER_TYPE_CONNECTION, "BrowserVirtualConnection", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
source_cnc_busy_cb (G_GNUC_UNUSED BrowserConnection *bcnc, gboolean is_busy,
		    G_GNUC_UNUSED const gchar *reason, BrowserConnection *virtual)
{
	if (BROWSER_VIRTUAL_CONNECTION (virtual)->priv->in_m_busy)
		return;
	g_signal_emit_by_name (virtual, "busy", is_busy,
			       is_busy ? _("Bound connection is used") : NULL);
}

static void
m_busy (BrowserConnection *bcnc, gboolean is_busy, const gchar *reason)
{
	/*
	 * declare all the source connections as busy
	 */
	GSList *list;
	if (! BROWSER_VIRTUAL_CONNECTION (bcnc)->priv->specs)
		return;

	BROWSER_VIRTUAL_CONNECTION (bcnc)->priv->in_m_busy = TRUE;

	for (list = BROWSER_VIRTUAL_CONNECTION (bcnc)->priv->specs->parts; list; list = list->next) {
		BrowserVirtualConnectionPart *part;
		part = (BrowserVirtualConnectionPart*) list->data;
		if (part->part_type == BROWSER_VIRTUAL_CONNECTION_PART_CNC) {
			BrowserVirtualConnectionCnc *cnc;
			cnc = &(part->u.cnc);
			g_signal_handlers_block_by_func (cnc->source_cnc,
							 G_CALLBACK (source_cnc_busy_cb),
							 bcnc);
			g_signal_emit_by_name (cnc->source_cnc, "busy", is_busy,
					       is_busy ? _("Virtual connection using this connection is busy") : NULL);
			g_signal_handlers_unblock_by_func (cnc->source_cnc,
							   G_CALLBACK (source_cnc_busy_cb),
							   bcnc);			
		}
	}

	BROWSER_CONNECTION_CLASS (parent_class)->busy (bcnc, is_busy, reason);

	BROWSER_VIRTUAL_CONNECTION (bcnc)->priv->in_m_busy = FALSE;
}

static void
browser_virtual_connection_class_init (BrowserVirtualConnectionClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	BROWSER_CONNECTION_CLASS (klass)->busy = m_busy;

	/* Properties */
        object_class->set_property = browser_virtual_connection_set_property;
        object_class->get_property = browser_virtual_connection_get_property;
	g_object_class_install_property (object_class, PROP_SPECS,
                                         g_param_spec_pointer ("specs", NULL,
							       "Specifications as a BrowserVirtualConnectionSpecs pointer",
							       G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY));
	object_class->dispose = browser_virtual_connection_dispose;
}

static void
browser_virtual_connection_init (BrowserVirtualConnection *bcnc)
{
	bcnc->priv = g_new0 (BrowserVirtualConnectionPrivate, 1);
}

static void
browser_virtual_connection_dispose (GObject *object)
{
	BrowserVirtualConnection *bcnc;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_VIRTUAL_CONNECTION (object));

	bcnc = BROWSER_VIRTUAL_CONNECTION (object);

	if (bcnc->priv) {
		if (bcnc->priv->specs) {
			GSList *list;
			for (list = bcnc->priv->specs->parts; list; list = list->next) {
				BrowserVirtualConnectionPart *part;
				part = (BrowserVirtualConnectionPart*) list->data;
				if (part->part_type == BROWSER_VIRTUAL_CONNECTION_PART_CNC) {
					BrowserVirtualConnectionCnc *cnc;
					cnc = &(part->u.cnc);
					g_signal_handlers_disconnect_by_func (cnc->source_cnc,
									      G_CALLBACK (source_cnc_busy_cb),
									      bcnc);
				}
			}
			browser_virtual_connection_specs_free (bcnc->priv->specs);
		}

		g_free (bcnc->priv);
		bcnc->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
browser_virtual_connection_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
        BrowserVirtualConnection *bcnc;

        bcnc = BROWSER_VIRTUAL_CONNECTION (object);
        if (bcnc->priv) {
                switch (param_id) {
                case PROP_SPECS:
			bcnc->priv->specs = browser_virtual_connection_specs_copy (g_value_get_pointer (value));
			GSList *list;
			for (list = bcnc->priv->specs->parts; list; list = list->next) {
				BrowserVirtualConnectionPart *part;
				part = (BrowserVirtualConnectionPart*) list->data;
				if (part->part_type == BROWSER_VIRTUAL_CONNECTION_PART_CNC) {
					BrowserVirtualConnectionCnc *cnc;
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
browser_virtual_connection_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
        BrowserVirtualConnection *bcnc;

        bcnc = BROWSER_VIRTUAL_CONNECTION (object);
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



typedef struct {
        guint cncid;
        GMainLoop *loop;
        GError **error;
	GdaThreadWrapper *wrapper;

        /* out */
        GdaConnection *cnc;
} MainloopData;

static gboolean
check_for_cnc (MainloopData *data)
{
        GdaConnection *cnc;
        GError *lerror = NULL;
        cnc = gda_thread_wrapper_fetch_result (data->wrapper, FALSE, data->cncid, &lerror);
        if (cnc || (!cnc && lerror)) {
                /* waiting is finished! */
                data->cnc = cnc;
                if (lerror)
                        g_propagate_error (data->error, lerror);
                g_main_loop_quit (data->loop);
                return FALSE;
        }
        return TRUE;
}

/*
 * executed in a sub thread
 */
static GdaConnection *
sub_thread_open_cnc (BrowserVirtualConnectionSpecs *specs, GError **error)
{
#ifndef DUMMY
	/* create GDA virtual connection */
	static GdaVirtualProvider *provider = NULL;
	GdaConnection *virtual;
	if (!provider)
		provider = gda_vprovider_hub_new ();

	virtual = gda_virtual_connection_open_extended (provider, GDA_CONNECTION_OPTIONS_THREAD_SAFE, NULL);
		
	/* add parts to connection as specified by @specs */
	GSList *list;
	for (list = specs->parts; list; list = list->next) {
		BrowserVirtualConnectionPart *part;
		part = (BrowserVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
			BrowserVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
								    pm->model, pm->table_name, error)) {
				g_object_unref (virtual);
				return NULL;
			}
			break;
		}
		case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
			BrowserVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual),
						      scnc->source_cnc->priv->cnc,
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
	
	return virtual;
#else
        sleep (5);
        g_set_error (error, 0, 0, "Timeout!!!");
        return NULL;
#endif
}

/**
 * browser_virtual_connection_new
 * @specs: the specifications of the virtual connection's contents
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #BrowserVirtualConnection connection.
 *
 * Returns: the new connection
 */
BrowserConnection *
browser_virtual_connection_new (const BrowserVirtualConnectionSpecs *specs, GError **error)
{
	/* open virtual GdaConnection in sub thread */
	GdaThreadWrapper *wrapper;
	guint cncid;

	g_return_val_if_fail (specs, NULL);

	wrapper = gda_thread_wrapper_new ();
	cncid = gda_thread_wrapper_execute (wrapper,
                                            (GdaThreadWrapperFunc) sub_thread_open_cnc,
                                            (gpointer) specs,
                                            (GDestroyNotify) NULL,
                                            error);
        if (cncid == 0)
                return NULL;

	GMainLoop *loop;
        guint source_id;
        MainloopData data;
	
        loop = g_main_loop_new (NULL, FALSE);
	data.wrapper = wrapper;
	data.cncid = cncid;
        data.error = error;
        data.loop = loop;
        data.cnc = NULL;

        source_id = g_timeout_add (200, (GSourceFunc) check_for_cnc, &data);
        g_main_loop_run (loop);
        g_main_loop_unref (loop);
	g_object_unref (wrapper);

	/* create the BrowserConnection object */
        if (data.cnc) {
		BrowserConnection *nbcnc;
                g_object_set (data.cnc, "monitor-wrapped-in-mainloop", TRUE, NULL);
		nbcnc = g_object_new (BROWSER_TYPE_VIRTUAL_CONNECTION, "specs", specs,
				      "gda-connection", data.cnc, NULL);
		g_object_unref (data.cnc);
		/*g_print ("BrowserVirtualConnection %p had TMP wrapper %p\n", nbcnc, wrapper);*/

		return nbcnc;
	}
	else
		return NULL;
}

/**
 * browser_virtual_connection_modify_specs
 * @bcnc: a #BrowserVirtualConnection connection
 * @new_specs: a pointer to a #BrowserVirtualConnectionSpecs for the new specs
 * @error: a place to store errors, or %NULL
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
browser_virtual_connection_modify_specs (BrowserVirtualConnection *bcnc,
					 const BrowserVirtualConnectionSpecs *new_specs, GError **error)
{
	g_return_val_if_fail (BROWSER_IS_VIRTUAL_CONNECTION (bcnc), FALSE);
	g_return_val_if_fail (new_specs, FALSE);

	/* undo the current specs */
	GSList *list;
	GdaConnection *cnc;
	cnc = g_object_get_data (G_OBJECT (BROWSER_CONNECTION (bcnc)->priv->cnc), "gda-virtual-connection");
	for (list = bcnc->priv->specs->parts; list; list = bcnc->priv->specs->parts) {
		BrowserVirtualConnectionPart *part;
		part = (BrowserVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
			BrowserVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (cnc),
								 pm->table_name, error))
				return FALSE;
			break;
		}
		case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
			BrowserVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_remove (GDA_VCONNECTION_HUB (cnc), scnc->source_cnc->priv->cnc,
							 error))
				return FALSE;
			break;
		}
		default:
			g_assert_not_reached ();
		}

		/* get rid of @part */
		browser_virtual_connection_part_free (part);
		bcnc->priv->specs->parts = g_slist_remove (bcnc->priv->specs->parts, part);
	}

	/* apply the new specs */
	browser_virtual_connection_specs_free (bcnc->priv->specs);
	bcnc->priv->specs = g_new0 (BrowserVirtualConnectionSpecs, 1);

	for (list = new_specs->parts; list; list = list->next) {
		BrowserVirtualConnectionPart *part;
		part = (BrowserVirtualConnectionPart*) list->data;
		switch (part->part_type) {
		case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
			BrowserVirtualConnectionModel *pm;
			pm = &(part->u.model);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc),
								    pm->model, pm->table_name, error))
				return FALSE;
			break;
		}
		case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
			BrowserVirtualConnectionCnc *scnc;
			scnc = &(part->u.cnc);
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (cnc), scnc->source_cnc->priv->cnc,
						      scnc->table_schema, error))
				return FALSE;
			break;
		}
		default:
			g_assert_not_reached ();
		}

		/* add @part to bcnc->priv->specs */
		bcnc->priv->specs->parts = g_slist_append (bcnc->priv->specs->parts,
							   browser_virtual_connection_part_copy (part));
	}
	
	return TRUE;
}


/*
 * Spec manipulations
 */

/**
 * browser_virtual_connection_part_copy
 */
BrowserVirtualConnectionPart *
browser_virtual_connection_part_copy (const BrowserVirtualConnectionPart *part)
{
	BrowserVirtualConnectionPart *npart;
	g_return_val_if_fail (part, NULL);

	npart = g_new0 (BrowserVirtualConnectionPart, 1);
	npart->part_type = part->part_type;

	switch (part->part_type) {
	case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
		const BrowserVirtualConnectionModel *spm;
		BrowserVirtualConnectionModel *npm;
		spm = &(part->u.model);
		npm = &(npart->u.model);
		if (spm->table_name)
			npm->table_name = g_strdup (spm->table_name);
		if (spm->model)
			npm->model = g_object_ref (G_OBJECT (spm->model));
		break;
	}
	case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
		const BrowserVirtualConnectionCnc *scnc;
		BrowserVirtualConnectionCnc *ncnc;
		scnc = &(part->u.cnc);
		ncnc = &(npart->u.cnc);
		if (scnc->table_schema)
			ncnc->table_schema = g_strdup (scnc->table_schema);
		if (scnc->source_cnc)
			ncnc->source_cnc = g_object_ref (G_OBJECT (scnc->source_cnc));
		break;
	}
	default:
		g_assert_not_reached ();
	}

	return npart;
}

/**
 * browser_virtual_connection_part_free
 */
void
browser_virtual_connection_part_free (BrowserVirtualConnectionPart *part)
{
	if (!part)
		return;

	switch (part->part_type) {
	case BROWSER_VIRTUAL_CONNECTION_PART_MODEL: {
		BrowserVirtualConnectionModel *pm;
		pm = &(part->u.model);
		g_free (pm->table_name);
		if (pm->model)
			g_object_unref (pm->model);
		break;
	}
	case BROWSER_VIRTUAL_CONNECTION_PART_CNC: {
		BrowserVirtualConnectionCnc *cnc;
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
 * browser_virtual_connection_specs_copy
 */
BrowserVirtualConnectionSpecs *
browser_virtual_connection_specs_copy (const BrowserVirtualConnectionSpecs *specs)
{
	BrowserVirtualConnectionSpecs *ns;
	GSList *list;

	g_return_val_if_fail (specs, NULL);

	ns = g_new0 (BrowserVirtualConnectionSpecs, 1);
	for (list = specs->parts; list; list = list->next) {
		BrowserVirtualConnectionPart *npart;
		npart = browser_virtual_connection_part_copy ((BrowserVirtualConnectionPart*) list->data);
		ns->parts = g_slist_prepend (ns->parts, npart);
	}
	ns->parts = g_slist_reverse (ns->parts);

	return ns;
}

/**
 * browser_virtual_connection_specs_free
 */
void
browser_virtual_connection_specs_free (BrowserVirtualConnectionSpecs *specs)
{
	if (!specs)
		return;
	g_slist_foreach (specs->parts, (GFunc) browser_virtual_connection_part_free, NULL);
	g_slist_free (specs->parts);
	g_free (specs);
}
