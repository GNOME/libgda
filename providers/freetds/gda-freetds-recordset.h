/* GDA DB FreeTDS provider
 * Copyright (C) 2002 - 2004 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#if !defined(__gda_freetds_recordset_h__)
#  define __gda_freetds_recordset_h__

#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-base.h>
#include <libgda/gda-value.h>
#include <tds.h>

G_BEGIN_DECLS

#define GDA_TYPE_FREETDS_RECORDSET            (gda_freetds_recordset_get_type())
#define GDA_FREETDS_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FREETDS_RECORDSET, GdaFreeTDSRecordset))
#define GDA_FREETDS_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FREETDS_RECORDSET, GdaFreeTDSRecordsetClass))
#define GDA_IS_FREETDS_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FREETDS_RECORDSET))
#define GDA_IS_FREETDS_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FREETDS_RECORDSET))

typedef struct _GdaFreeTDSRecordset        GdaFreeTDSRecordset;
typedef struct _GdaFreeTDSRecordsetClass   GdaFreeTDSRecordsetClass;
typedef struct _GdaFreeTDSRecordsetPrivate GdaFreeTDSRecordsetPrivate;

struct _GdaFreeTDSRecordset {
	GdaDataModelBase model;
	GdaFreeTDSRecordsetPrivate *priv;
};

struct _GdaFreeTDSRecordsetClass {
	GdaDataModelBaseClass parent_class;
};

struct _GdaFreeTDSRecordsetPrivate {
	GdaConnection            *cnc;
	GdaFreeTDSConnectionData *tds_cnc;

	TDS_INT                  rc;
	TDSRESULTINFO            *res;
	guint                    colcnt;
	guint                    rowcnt;
	gboolean                 fetched_all_results;

	GPtrArray                *columns;
	GPtrArray                *rows;
};

GType          gda_freetds_recordset_get_type (void);
GdaDataModel  *gda_freetds_recordset_new (GdaConnection *cnc,
                                          gboolean fetchall);


G_END_DECLS

#endif

