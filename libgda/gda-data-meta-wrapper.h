/* GDA common library
 * Copyright (C) 2009 The GNOME Foundation.
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

#ifndef __GDA_DATA_META_WRAPPER_H__
#define __GDA_DATA_META_WRAPPER_H__

#include <libgda/gda-data-model.h>
#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_META_WRAPPER            (_gda_data_meta_wrapper_get_type())
#define GDA_DATA_META_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_META_WRAPPER, GdaDataMetaWrapper))
#define GDA_DATA_META_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_META_WRAPPER, GdaDataMetaWrapperClass))
#define GDA_IS_DATA_META_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_META_WRAPPER))
#define GDA_IS_DATA_META_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_META_WRAPPER))

typedef struct _GdaDataMetaWrapper        GdaDataMetaWrapper;
typedef struct _GdaDataMetaWrapperClass   GdaDataMetaWrapperClass;
typedef struct _GdaDataMetaWrapperPrivate GdaDataMetaWrapperPrivate;

typedef enum {
	GDA_DATA_META_WRAPPER_MODE_LC, /* quote identifier if there are some non lower case chars */
	GDA_DATA_META_WRAPPER_MODE_UC  /* quote identifier if there are some non upper case chars */
} GdaDataMetaWrapperMode;

struct _GdaDataMetaWrapper {
	GObject                        object;
	GdaDataMetaWrapperPrivate   *priv;
};

struct _GdaDataMetaWrapperClass {
	GObjectClass                   parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         _gda_data_meta_wrapper_get_type    (void) G_GNUC_CONST;
GdaDataModel *_gda_data_meta_wrapper_new         (GdaDataModel *model, gboolean reuseable,
						  gint *cols, gint size, GdaSqlIdentifierStyle mode,
						  GdaSqlReservedKeywordsFunc reserved_keyword_func);

G_END_DECLS

#endif
