/* GDA SQLite vprovider for Hub of connections
 * Copyright (C) 2007 - 2009 The GNOME Foundation.
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

#ifndef __GDA_VPROVIDER_HUB_H__
#define __GDA_VPROVIDER_HUB_H__

#include <virtual/gda-vprovider-data-model.h>

#define GDA_TYPE_VPROVIDER_HUB            (gda_vprovider_hub_get_type())
#define GDA_VPROVIDER_HUB(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VPROVIDER_HUB, GdaVproviderHub))
#define GDA_VPROVIDER_HUB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VPROVIDER_HUB, GdaVproviderHubClass))
#define GDA_IS_VPROVIDER_HUB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VPROVIDER_HUB))
#define GDA_IS_VPROVIDER_HUB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VPROVIDER_HUB))

G_BEGIN_DECLS

typedef struct _GdaVproviderHub      GdaVproviderHub;
typedef struct _GdaVproviderHubClass GdaVproviderHubClass;
typedef struct _GdaVproviderHubPrivate GdaVproviderHubPrivate;

struct _GdaVproviderHub {
	GdaVproviderDataModel      parent;
	GdaVproviderHubPrivate    *priv;
};

struct _GdaVproviderHubClass {
	GdaVproviderDataModelClass parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType               gda_vprovider_hub_get_type (void) G_GNUC_CONST;
GdaVirtualProvider *gda_vprovider_hub_new      (void);

G_END_DECLS

#endif
