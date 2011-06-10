/*
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_HANDLER_NUMERICAL__
#define __GDA_HANDLER_NUMERICAL__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_HANDLER_NUMERICAL          (gda_handler_numerical_get_type())
#define GDA_HANDLER_NUMERICAL(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_handler_numerical_get_type(), GdaHandlerNumerical)
#define GDA_HANDLER_NUMERICAL_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_handler_numerical_get_type (), GdaHandlerNumericalClass)
#define GDA_IS_HANDLER_NUMERICAL(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_handler_numerical_get_type ())

typedef struct _GdaHandlerNumerical      GdaHandlerNumerical;
typedef struct _GdaHandlerNumericalClass GdaHandlerNumericalClass;
typedef struct _GdaHandlerNumericalPriv  GdaHandlerNumericalPriv;


/* struct for the object's data */
struct _GdaHandlerNumerical
{
	GObject                   object;
	GdaHandlerNumericalPriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerNumericalClass
{
	GObjectClass              parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

/**
 * SECTION:gda-handler-numerical
 * @short_description: Default handler for numeric values
 * @title: GdaHanderNumerical
 * @stability: Stable
 * @see_also: #GdaDataHandler interface
 *
 * You should normally not need to use this API, refer to the #GdaDataHandler
 * interface documentation for more information.
 */

GType           gda_handler_numerical_get_type      (void) G_GNUC_CONST;
GdaDataHandler *gda_handler_numerical_new           (void);

G_END_DECLS

#endif
