/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VCONNECTION_HUB_H__
#define __GDA_VCONNECTION_HUB_H__

#include "gda-vconnection-data-model.h"

#define GDA_TYPE_VCONNECTION_HUB            (gda_vconnection_hub_get_type())

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE (GdaVconnectionHub, gda_vconnection_hub, GDA, VCONNECTION_HUB, GdaVconnectionDataModel)

struct _GdaVconnectionHubClass {
	GdaVconnectionDataModelClass parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

typedef void (*GdaVConnectionHubFunc) (GdaConnection *cnc, const gchar *ns, gpointer data);

/**
 * SECTION:gda-vconnection-hub
 * @short_description: Virtual connection which bind together connections
 * @title: GdaVconnectionHub
 * @stability: Stable
 * @see_also: 
 *
 * The #GdaVconnectionHub object "binds" together the tables from other (opened) connections to make it possible to run
 * SQL queries on data from several connections at once.
 *
 * A #GdaVconnectionHub connection can bind several other connections, each separated in its own namespace (which is specified
 * when adding a connection using gda_vconnection_hub_add()).
 *
 * For example if a connection A has two tables 'table_1' and 'table_2', then after gda_vconnection_hub_add() has been called
 * with A as connection argument and with a "c1" namespace, then in the corresponding #GdaVconnectionHub connection, table 
 * 'table_1' must be referred to as 'c1.table_1' and 'table_2' must be referred to as 'c1.table_2'.
 */

gboolean            gda_vconnection_hub_add            (GdaVconnectionHub *hub,
							GdaConnection *cnc, const gchar *ns, GError **error);
gboolean            gda_vconnection_hub_remove         (GdaVconnectionHub *hub, GdaConnection *cnc, GError **error);
GdaConnection      *gda_vconnection_hub_get_connection (GdaVconnectionHub *hub, const gchar *ns);
void                gda_vconnection_hub_foreach        (GdaVconnectionHub *hub, 
							GdaVConnectionHubFunc func, gpointer data);

G_END_DECLS

#endif
