/* GDA
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifndef __GDA_VCONNECTION_HUB_H__
#define __GDA_VCONNECTION_HUB_H__

#include <virtual/gda-vconnection-data-model.h>

#define GDA_TYPE_VCONNECTION_HUB            (gda_vconnection_hub_get_type())
#define GDA_VCONNECTION_HUB(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VCONNECTION_HUB, GdaVconnectionHub))
#define GDA_VCONNECTION_HUB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VCONNECTION_HUB, GdaVconnectionHubClass))
#define GDA_IS_VCONNECTION_HUB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VCONNECTION_HUB))
#define GDA_IS_VCONNECTION_HUB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VCONNECTION_HUB))

G_BEGIN_DECLS

typedef struct _GdaVconnectionHub      GdaVconnectionHub;
typedef struct _GdaVconnectionHubClass GdaVconnectionHubClass;
typedef struct _GdaVconnectionHubPrivate GdaVconnectionHubPrivate;

typedef void (*GdaVConnectionHubFunc) (GdaConnection *cnc, const gchar *ns, gpointer data);

struct _GdaVconnectionHub {
	GdaVconnectionDataModel      parent;
	GdaVconnectionHubPrivate    *priv;
};

struct _GdaVconnectionHubClass {
	GdaVconnectionDataModelClass parent_class;
};

GType               gda_vconnection_hub_get_type       (void) G_GNUC_CONST;

gboolean            gda_vconnection_hub_add            (GdaVconnectionHub *hub, 
							GdaConnection *cnc, const gchar *ns, GError **error);
gboolean            gda_vconnection_hub_remove         (GdaVconnectionHub *hub, GdaConnection *cnc, GError **error);
GdaConnection      *gda_vconnection_hub_get_connection (GdaVconnectionHub *hub, const gchar *ns);
void                gda_vconnection_hub_foreach        (GdaVconnectionHub *hub, 
							GdaVConnectionHubFunc func, gpointer data);

G_END_DECLS

#endif
