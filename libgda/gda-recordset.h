/* GDA library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999-2001 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_recordset_h__)
#  define __gda_recordset_h__

#include <libgda/gda-data-model.h>
#include <libgda/gda-field.h>

G_BEGIN_DECLS

typedef struct _GdaRecordset      GdaRecordset;
typedef struct _GdaRecordsetClass GdaRecordsetClass;

#define GDA_TYPE_RECORDSET            (gda_recordset_get_type())
#define GDA_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_RECORDSET, GdaRecordset))
#define GDA_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_RECORDSET, GdaRecordsetClass))
#define GDA_IS_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_RECORDSET))
#define GDA_IS_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_RECORDSET))

struct _GdaRecordset {
	GdaDataModel model;
};

struct _GdaRecordsetClass {
	GdaDataModelClass parent_class;
};

GType         gda_recordset_get_type (void);

GdaRecordset *gda_recordset_new (void);

G_END_DECLS

#endif
