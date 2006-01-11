/* GDA Sybase provider
 * Copyright (C) 1998 - 2004 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
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

#if !defined(__gda_sybase_recordset_h__)
#  define __gda_sybase_recordset_h__

#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-row.h>
#include <libgda/gda-value.h>
#include <ctpublic.h>
#include <cspublic.h>

#include "gda-sybase-provider.h"

G_BEGIN_DECLS

#define GDA_TYPE_SYBASE_RECORDSET            (gda_sybase_recordset_get_type())
#define GDA_SYBASE_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SYBASE_RECORDSET, GdaSybaseRecordset))
#define GDA_SYBASE_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SYBASE_RECORDSET, GdaSybaseRecordsetClass))
#define GDA_IS_SYBASE_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SYBASE_RECORDSET))
#define GDA_IS_SYBASE_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_SYBASE_RECORDSET))

typedef struct _GdaSybaseRecordset        GdaSybaseRecordset;
typedef struct _GdaSybaseRecordsetClass   GdaSybaseRecordsetClass;
typedef struct _GdaSybaseField            GdaSybaseField;
typedef struct _GdaSybaseRecordsetPrivate GdaSybaseRecordsetPrivate;

struct _GdaSybaseRecordset {
	GdaDataModelRow model;
	GdaSybaseRecordsetPrivate *priv;
};

struct _GdaSybaseRecordsetClass {
	GdaDataModelRowClass parent_class;
};

struct _GdaSybaseField {
	CS_SMALLINT indicator;
	gchar       *data;
	CS_INT      datalen;
	CS_DATAFMT  fmt;
};

struct _GdaSybaseRecordsetPrivate {
	GdaConnection           *cnc;
	GdaSybaseConnectionData *scnc;

	CS_INT                  rc;
	guint                   colcnt;
	guint                   rowcnt;
	gboolean                fetched_all_results;

	GPtrArray               *columns;
	GPtrArray               *rows;
};

GType               gda_sybase_recordset_get_type (void);

GdaSybaseRecordset *gda_sybase_process_row_result (GdaConnection           *cnc,
						   GdaSybaseConnectionData *scnc,
						   gboolean                fetchall);
GdaSybaseRecordset *gda_sybase_process_msg_result (GdaConnection *cnc,
						   GdaSybaseConnectionData *scnc);


G_END_DECLS

#endif
