/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_listener_h__)
#  define __gda_listener_h__

#include <glib.h>
#include <bonobo/bonobo-xobject.h>
#include <GDA.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct _GdaListener GdaListener;
	typedef struct _GdaListenerClass GdaListenerClass;
	typedef struct _GdaListenerPrivate GdaListenerPrivate;

#define GDA_TYPE_LISTENER            (gda_listener_get_type ())
#define GDA_LISTENER(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_LISTENER, GdaListener)
#define GDA_LISTENER_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_LISTENER, GdaListenerClass)
#define GDA_IS_LISTENER(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_LISTENER)
#define GDA_IS_LISTENER_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_LISTENER))

	struct _GdaListener
	{
		BonoboXObject object;
		GdaListenerPrivate *priv;
	};

	struct _GdaListenerClass
	{
		BonoboXObjectClass parent_class;
		POA_GDA_Listener__epv epv;

		/* signals */
		void (*notify_action) (GdaListener * listener,
				       const gchar * message,
				       GDA_ListenerAction action,
				       const gchar * description);
	};

	GtkType gda_listener_get_type (void);
	GdaListener *gda_listener_new (void);

	void gda_listener_notify_action (GdaListener * listener,
					 const gchar * message,
					 GDA_ListenerAction action,
					 const gchar * description);

#ifdef __cplusplus
}
#endif

#endif
