/* GDA FireBird Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_firebird_recordset_h__)
#  define __gda_firebird_recordset_h__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_RECORDSET            (gda_firebird_recordset_get_type())
#define GDA_FIREBIRD_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordset))
#define GDA_FIREBIRD_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordsetClass))
#define GDA_IS_FIREBIRD_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_RECORDSET))
#define GDA_IS_FIREBIRD_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_RECORDSET))

typedef struct _GdaFirebirdRecordset      GdaFirebirdRecordset;
typedef struct _GdaFirebirdRecordsetClass GdaFirebirdRecordsetClass;

struct _GdaFirebirdRecordset {
	GdaDataModel model;
};

struct _GdaFirebirdRecordsetClass {
	GdaDataModelClass parent_class;
};

G_END_DECLS

#endif
